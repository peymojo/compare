//
//    compare
//    Copyright (C) 2018 Paul Young (aka peymojo)
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <iostream>
#include <list>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include "Hermit/File/CompareFiles.h"
#include "Hermit/File/CreateFilePathFromUTF8String.h"
#include "Hermit/File/FileExists.h"
#include "Hermit/File/FileNotification.h"
#include "Hermit/File/GetFilePathUTF8String.h"
#include "Hermit/Foundation/LoggingHermit.h"
#include "Hermit/String/SimplifyPath.h"

namespace compare_Impl {

    //
    class Hermit : public hermit::Hermit {
    public:
        //
        Hermit(const hermit::HermitPtr& h_) : mH_(h_) {
        }
        
        //
        virtual bool ShouldAbort() override {
            return mH_->ShouldAbort();
        }
        
        //
        virtual void Notify(const char* notificationName, const void* param) override {
            std::lock_guard<std::mutex> guard(mMutex);
            
            std::string name(notificationName);
            
            if ((name == hermit::file::kFilesMatchNotification) ||
                (name == hermit::file::kFilesDifferNotification) ||
                (name == hermit::file::kFileSkippedNotification) ||
                (name == hermit::file::kFileErrorNotification)) {
                hermit::file::FileNotificationParams* params = (hermit::file::FileNotificationParams*)param;
                
                std::string path1UTF8;
                if (params->mPath1 != nullptr) {
                    hermit::file::GetFilePathUTF8String(mH_, params->mPath1, path1UTF8);
                }
                std::string path2UTF8;
                if (params->mPath2 != nullptr) {
                    hermit::file::GetFilePathUTF8String(mH_, params->mPath2, path2UTF8);
                }
                
                if (name == hermit::file::kFilesMatchNotification) {
                    std::cout << "Match: " << path1UTF8 << std::endl;
                }
                else if (name == hermit::file::kFilesDifferNotification) {
                    std::cout << "Different: " << path1UTF8 << " (" << params->mType << ")" << std::endl;
                    if (params->mType == hermit::file::kCreationDatesDiffer) {
                        std::cout << "\t" << "Date 1: " << params->mString1 << std::endl;
                        std::cout << "\t" << "Date 2: " << params->mString2 << std::endl;
                    }
                    else if (params->mType == hermit::file::kXAttrPresenceMismatch) {
                        if (!params->mString1.empty()) {
                            std::cout << "\t" << "Only in 1: " << params->mString1 << std::endl;
                        }
                        else {
                            std::cout << "\t" << "Only in 2: " << params->mString2 << std::endl;
                        }
                    }
                }
                else if (name == hermit::file::kFileSkippedNotification) {
                    std::cout << "Skipped: " << path1UTF8 << std::endl;
                }
                else {
                    std::cout << "ERROR: " << path1UTF8 << std::endl;
                }
            }
            else {
                NOTIFY(mH_, notificationName, param);
            }
        }
        
        //
        hermit::HermitPtr mH_;
        std::mutex mMutex;
    };

    //
    typedef std::set<std::string> StringSet;
    
    //
    class Preprocessor : public hermit::file::PreprocessFileFunction {
    public:
        //
        Preprocessor(const StringSet& exclusions) :
        mExclusions(exclusions) {
        }
        
        //
        virtual hermit::file::PreprocessFileInstruction Preprocess(const hermit::HermitPtr& h_,
                                                                   const hermit::file::FilePathPtr& parent,
                                                                   const std::string& itemName) override {
            if (mExclusions.find(itemName) != mExclusions.end()) {
                return hermit::file::PreprocessFileInstruction::kSkip;
            }
            return hermit::file::PreprocessFileInstruction::kContinue;

        }
        
        //
        StringSet mExclusions;
    };
    
    //
    class CompareCompletion : public hermit::file::CompareFilesCompletion {
    public:
        //
        CompareCompletion() : mStatus(hermit::file::CompareFilesStatus::kUnknown) {
        }
        
        //
        virtual void Call(const hermit::file::CompareFilesStatus& status) override {
            mStatus = status;
        }
        
        //
        bool Done() {
            return (mStatus != hermit::file::CompareFilesStatus::kUnknown);
        }
        
        //
        std::atomic<hermit::file::CompareFilesStatus> mStatus;
    };
    
    //
    int compare(const std::string& path1, const std::string& path2, bool ignoreDates, bool ignoreFinderInfo, bool summarize) {
        auto h_ = std::make_shared<Hermit>(std::make_shared<hermit::LoggingHermit>());

        std::vector<char> wdBuf(2048);
        std::string workingDir;
        const char* cwd = getcwd(&wdBuf.at(0), 2048);
        if (cwd == 0) {
            std::cout << "WARNING: Current working directory appears invalid. (Was this directory deleted?)" << "\n";
        }
        else {
            workingDir = cwd;
        }
        
        std::string simplifiedPath1;
        if (!hermit::string::SimplifyPath(h_, path1, workingDir, simplifiedPath1)) {
            NOTIFY_ERROR(h_, "SimplifyPath failed for:", path1);
            return EXIT_FAILURE;
        }
        hermit::file::FilePathPtr filePath1;
        hermit::file::CreateFilePathFromUTF8String(h_, simplifiedPath1, filePath1);

        std::string simplifiedPath2;
        if (!hermit::string::SimplifyPath(h_, path2, workingDir, simplifiedPath2)) {
            NOTIFY_ERROR(h_, "SimplifyPath failed for:", path2);
            return EXIT_FAILURE;
        }
        hermit::file::FilePathPtr filePath2;
        hermit::file::CreateFilePathFromUTF8String(h_, simplifiedPath2, filePath2);

        hermit::file::FileExistsCallbackClass exists1;
        hermit::file::FileExists(h_, filePath1, exists1);
        if (!exists1.mSuccess) {
            NOTIFY_ERROR(h_, "FileExists failed for:", filePath1);
            return EXIT_FAILURE;
        }
        if (!exists1.mExists) {
            std::cout << "compare: Item 1 doesn't exist at path: <" << path1 << ">\n";
            return EXIT_FAILURE;
        }
        
        hermit::file::FileExistsCallbackClass exists2;
        hermit::file::FileExists(h_, filePath2, exists2);
        if (!exists2.mSuccess) {
            NOTIFY_ERROR(h_, "FileExists failed for:", filePath2);
            return EXIT_FAILURE;
        }
        if (!exists2.mExists) {
            std::cout << "compare: Item 2 doesn't exist at path: <" << path2 << ">\n";
            return EXIT_FAILURE;
        }
        
        StringSet filenamesToSkip;
        filenamesToSkip.insert(".DS_Store");                // Finder view file which gets added/updated when you open a folder
        filenamesToSkip.insert(".ipspot_update");           // Spotlight photo data file which the OS changes on its own
        filenamesToSkip.insert("ehthumbs.db");              // Windows thumbnails file
        filenamesToSkip.insert("ehthumbs_vista.db");        // Windows thumbnails file
        filenamesToSkip.insert("Thumbs.db");                // Windows thumbnails file
        
        auto preprocessor = std::make_shared<Preprocessor>(filenamesToSkip);
        auto completion = std::make_shared<CompareCompletion>();
        hermit::file::CompareFiles(h_,
                                   filePath1,
                                   filePath2,
								   std::make_shared<hermit::file::HardLinkMap>(filePath1),
								   std::make_shared<hermit::file::HardLinkMap>(filePath2),
                                   ignoreDates,
                                   ignoreFinderInfo,
                                   preprocessor,
                                   completion);
        while (!completion->Done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return 0;
    }

} // namespace compare_Impl
using namespace compare_Impl;

//
int main(int argc, const char * argv[]) {
    std::list<std::string> args;
    for (int n = 1; n < argc; ++n) {
        args.push_back(argv[n]);
    }
    
    if (args.size() < 2) {
        std::cout << "usage: compare [options] <path_1> <path_2>\n";
        std::cout << "[options]:" << "\n";
        std::cout << "\t-d ignore creation/modification dates when comparing items" << "\n";
        std::cout << "\t-f ignore finder info when comparing items" << "\n";
        std::cout << "\t-s summarize differences without going into detail" << "\n";
        return EXIT_FAILURE;
    }
    
    bool ignoreDates = false;
    bool ignoreFinderInfo = false;
    bool summarize = false;
    std::string path1;
    std::string path2;
    while (!args.empty()) {
        std::string arg(args.front());
        args.pop_front();
        if (arg == "-d") {
            ignoreDates = true;
        }
        else if (arg == "-f") {
            ignoreFinderInfo = true;
        }
        else if (arg == "-s") {
            summarize = true;
        }
        else if (path1.empty()) {
            path1 = arg;
        }
        else if (path2.empty()) {
            path2 = arg;
        }
    }
    return compare(path1, path2, ignoreDates, ignoreFinderInfo, summarize);
}

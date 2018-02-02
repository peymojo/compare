//
//	Hermit
//	Copyright (C) 2017 Paul Young (aka peymojo)
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <iostream>
#include <thread>
#include "Hermit/File/ReadFirstLineFromUTF8FilePath.h"
#include "ReadKeyFile.h"

namespace hermit {
	namespace ReadKeyFile_Impl {
		//
		class Completion : public file::ReadFirstLineFromUTF8FilePathCompletion {
		public:
			//
			Completion() : mResult(file::ReadFirstLineFromUTF8FilePathResult::kUnknown) {
			}
			
			//
			virtual void Call(const HermitPtr& h_,
							  const file::ReadFirstLineFromUTF8FilePathResult& result,
							  const std::string& line) {
				mResult = result;
				if (result == file::ReadFirstLineFromUTF8FilePathResult::kSuccess) {
					mLine = line;
				}
			}
			
			//
			std::atomic<file::ReadFirstLineFromUTF8FilePathResult> mResult;
			std::string mLine;
		};
		
	} // namespace ReadKeyFile_Impl
	using namespace ReadKeyFile_Impl;
	
	//
	bool ReadKeyFile(const HermitPtr& h_, const char* pathToKeyFileUTF8, std::string& outKey) {
		auto completion = std::make_shared<Completion>();
		file::ReadFirstLineFromUTF8FilePath(h_, pathToKeyFileUTF8, completion);
		while (completion->mResult == file::ReadFirstLineFromUTF8FilePathResult::kUnknown) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		if (completion->mResult == file::ReadFirstLineFromUTF8FilePathResult::kFileNotFound) {
			std::cout << "s3util: No key file found at path: " << pathToKeyFileUTF8 << "\n";
			return false;
		}
		if (completion->mResult != file::ReadFirstLineFromUTF8FilePathResult::kSuccess) {
			std::cout << "s3util: Error reading key file at path: " << pathToKeyFileUTF8 << "\n";
			return false;
		}
		outKey = completion->mLine;
		return true;
	}
	
} // namespace hermit


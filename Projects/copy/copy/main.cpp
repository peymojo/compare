//
//    copy
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

#include <iomanip>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Hermit/Encoding/CalculateDataCRC32.h"
#include "Hermit/File/AppendToFilePath.h"
#include "Hermit/File/CompareFiles.h"
#include "Hermit/File/CreateFilePathFromUTF8String.h"
#include "Hermit/File/GetFileType.h"
#include "Hermit/File/GetFilePathLeaf.h"
#include "Hermit/File/GetFilePathParent.h"
#include "Hermit/File/GetFilePathUTF8String.h"
#include "Hermit/File/FileExists.h"
#include "Hermit/File/FileNotification.h"
#include "Hermit/File/FileSystemCopy.h"
#include "Hermit/File/PathIsDirectory.h"
//#include "Hermit/File/StreamInFileForkRange.h"
#include "Hermit/Foundation/CompareMemory.h"
#include "Hermit/Foundation/LoggingHermit.h"
#include "Hermit/Foundation/Notification.h"
#include "Hermit/String/AddTrailingSlash.h"
#include "Hermit/String/GetCommonPathParent.h"
#include "Hermit/String/GetRelativePath.h"
#include "Hermit/String/SimplifyPath.h"
#include "Hermit/String/SInt32ToString.h"
#include "Hermit/String/UInt32ToString.h"
#include "Hermit/String/UInt64ToString.h"
#include "Hermit/Utility/OperationTimer.h"

namespace copy_Impl {
	
	//
	class CoutReporter {
	public:
		//
		void Report(const std::string& inTag, const time_t& inTime) {
			static const time_t oneMinute = 60;
			static const time_t oneHour = 60 * oneMinute;
			static const time_t oneDay = oneHour * 24;
			static const time_t oneYear = oneDay * 365;
			
			time_t t = inTime;
			if (inTime < 60) {
				std::cout << inTag << ": " << inTime << " seconds." << "\n";
			}
			else {
				std::ostringstream formattedTimeStream;
				if (t >= oneYear) {
					time_t years = t / oneYear;
					formattedTimeStream << years << " year";
					if (years > 1) {
						formattedTimeStream << "s";
					}
					formattedTimeStream << " ";
					
					t %= oneYear;
				}
				if (t >= oneDay) {
					time_t days = t / oneDay;
					formattedTimeStream << days << " day";
					if (days > 1) {
						formattedTimeStream << "s";
					}
					formattedTimeStream << " ";
					
					t %= oneDay;
				}
				if (t >= oneHour) {
					time_t hours = t / oneHour;
					formattedTimeStream << hours << " hour";
					if (hours > 1) {
						formattedTimeStream << "s";
					}
					formattedTimeStream << " ";
					
					t %= oneHour;
				}
				if (t >= oneMinute) {
					time_t minutes = t / oneMinute;
					formattedTimeStream << minutes << " minute";
					if (minutes > 1) {
						formattedTimeStream << "s";
					}
					formattedTimeStream << " ";
					
					t %= oneMinute;
				}
				if (t > 0) {
					formattedTimeStream << t << " second";
					if (t > 1) {
						formattedTimeStream << "s";
					}
					formattedTimeStream << " ";
				}
				
				std::cout << inTag << ": " << formattedTimeStream.str() << "(" << inTime << " seconds).\n";
			}
		}
	};
	
	//
	typedef hermit::utility::OperationTimer<CoutReporter> Timer;
	
	//
	void usage() {
		std::cout << "usage: copy <source> <destination>\n";
		std::cout << "\t-y verify results after copy\n";
		//        std::cout << "\t-v verbose\n";
	}
	
#if 000
	//
	class Logger
	{
	public:
		//
		//
		Logger(
			   bool inVerbose)
		:
		mVerbose(inVerbose)
		{
		}
		
		//
		//
		void Log(
				 const hermit::ConstCharPtr& inMessage)
		{
			std::cout << inMessage << "\n";
			mErrors.push_back(inMessage);
		}
		
		//
		//
		void PrintErrors()
		{
			if (!mErrors.empty())
			{
				std::cout << "\n-----\nThere were errors:\n";
				
				std::vector<std::string>::const_iterator end = mErrors.end();
				for (std::vector<std::string>::const_iterator it = mErrors.begin(); it != end; ++it)
				{
					std::cout << *it << "\n";
				}
				
				std::cout << "-----\n\n";
			}
		}
		
		//
		//
		bool mVerbose;
		std::vector<std::string> mErrors;
	};
	
	//
	//
	static Logger sLogger(true);
#endif
	
	//
	typedef std::vector<std::string> StringVector;
	
	//
	class IntermediateUpdateCallback : public hermit::file::FileSystemCopyIntermediateUpdateCallback {
	public:
		//
		IntermediateUpdateCallback() {
		}
		
		//
		virtual bool OnUpdate(const hermit::HermitPtr& h_,
							  const hermit::file::FileSystemCopyResult& result,
							  const hermit::file::FilePathPtr& sourcePath,
							  const hermit::file::FilePathPtr& destPath) override {
			if (result == hermit::file::FileSystemCopyResult::kSuccess) {
				std::string sourcePathUTF8;
				hermit::file::GetFilePathUTF8String(h_, sourcePath, sourcePathUTF8);
				std::cout << "Copied " << sourcePathUTF8 << "\n";
			}
			else {
				std::string sourcePathUTF8;
				hermit::file::GetFilePathUTF8String(h_, sourcePath, sourcePathUTF8);
				std::cout << "ERROR copying " << sourcePathUTF8 << "\n";
				mErrors.push_back(sourcePathUTF8);
			}
			return true;
		}
		
		//
		StringVector mErrors;
	};
	
	//
	class CopyCompletion : public hermit::file::FileSystemCopyCompletion {
	public:
		//
		CopyCompletion() : mResult(hermit::file::FileSystemCopyResult::kUnknown) {
		}
		
		//
		virtual void Call(const hermit::HermitPtr& h_, const hermit::file::FileSystemCopyResult& result) override {
			mResult = result;
		}
		
		//
		bool Done() {
			return (mResult != hermit::file::FileSystemCopyResult::kUnknown);
		}
		
		//
		std::atomic<hermit::file::FileSystemCopyResult> mResult;
	};
	
#if 000
	//
	class StreamInCallback : public hermit::file::StreamInFileForkRangeFunction {
	public:
		StreamInCallback() : mSuccess(false) {
		}
		
		bool Function(const bool& inSuccess,
					  const hermit::ConstCharPtr& inData,
					  const uint64_t& inDataSize,
					  const bool& inEndOfStream) {
			mSuccess = inSuccess;
			if (inSuccess) {
				mData += std::string(inData, inDataSize);
			}
			return true;
		}
		
		bool mSuccess;
		std::string mData;
	};
#endif
	
#if 000
	//
	bool DisplayByteRange(hermit::file::FilePathPtr inFilePath,
						  const char* inForkName,
						  uint64_t inStart,
						  uint64_t inEnd) {
		StreamInCallback streamCallback;
		hermit::file::StreamInFileForkRange(
											inFilePath,
											inForkName,
											inStart,
											inEnd,
											512,
											streamCallback);
		
		if (!streamCallback.mSuccess)
		{
			hermit::file::LogFilePath("DisplayByteRange(): StreamInFileForkRange failed for path: ", inFilePath);
			return false;
		}
		
		for (std::string::size_type n = 0; n < streamCallback.mData.size(); ++n)
		{
			std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)streamCallback.mData[n] << std::dec << " ";
			char ch = streamCallback.mData[n];
			if ((ch < 32) || (ch > 126))
			{
				ch = '?';
			}
			std::cout << "(" << ch << ") ";
			if (((n + 1) % 16) == 0)
			{
				std::cout << "\n";
			}
		}
		if ((streamCallback.mData.size() % 16) != 0)
		{
			std::cout << "\n";
		}
		return true;
	}
#endif
	
	//
	std::string SanitizeStringForOutput(const std::string& inString) {
		std::string result;
		for (std::string::size_type n = 0; n < inString.size(); ++n) {
			std::string::value_type ch = inString[n];
			if ((ch >= 0) && (ch < 32)) {
				std::ostringstream strm;
				strm << "{0x" << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)ch << "}";
				result += strm.str();
			}
			else {
				result += ch;
			}
		}
		return result;
	}
	
#if 000
	//
	class UInt64ValueCallback
	:
	public hermit::GetNotificationValueCallback
	{
	public:
		//
		//
		UInt64ValueCallback()
		:
		mValue(0)
		{
		}
		
		//
		//
		bool Function(
					  const bool& inSuccess,
					  const hermit::ConstCharPtr& inKey,
					  const hermit::ConstCharPtr& inValueType,
					  const hermit::ConstVoidPtr& inValuePtr)
		{
			if (inSuccess && (strcmp(inValueType, "uint64") == 0) && (inValuePtr != nullptr))
			{
				mValue = *(const uint64_t*)inValuePtr;
			}
			return true;
		}
		
		//
		//
		uint64_t mValue;
	};
#endif
	
#if 000
	//
	//
	class StringValueCallback
	:
	public hermit::GetNotificationValueCallback
	{
	public:
		//
		//
		bool Function(
					  const bool& inSuccess,
					  const hermit::ConstCharPtr& inKey,
					  const hermit::ConstCharPtr& inValueType,
					  const hermit::ConstVoidPtr& inValuePtr)
		{
			if (inSuccess && (strcmp(inValueType, "string") == 0) && (inValuePtr != nullptr))
			{
				mValue = (const char*)inValuePtr;
			}
			return true;
		}
		
		//
		//
		std::string mValue;
	};
#endif
	
#if 000
	//
	//
	class FilePathValueCallback
	:
	public hermit::GetNotificationValueCallback
	{
	public:
		//
		//
		bool Function(
					  const bool& inSuccess,
					  const hermit::ConstCharPtr& inKey,
					  const hermit::ConstCharPtr& inValueType,
					  const hermit::ConstVoidPtr& inValuePtr)
		{
			if (inSuccess && (strcmp(inValueType, "filepath") == 0) && (inValuePtr != nullptr))
			{
				mFilePath = (const hermit::file::FilePathPtr)inValuePtr;
			}
			return true;
		}
		
		//
		//
		hermit::file::ManagedFilePathPtr mFilePath;
	};
#endif
	
	//
	class Hermit : public hermit::Hermit {
	public:
		//
		Hermit(const hermit::HermitPtr& h_) : mH_(h_), mFirstDifferentByte(0) {
		}
		
		//
		virtual bool ShouldAbort() override {
			return mH_->ShouldAbort();
		}
		
		//
		virtual void Notify(const char* notificationName, const void* param) override {
#if 000
			std::string name(inName);
			if ((name == hermit::file::kFilesMatchNotification) ||
				(name == hermit::file::kFilesDifferNotification)) {
				StringValueCallback notificationType;
				inGetValueFunction.Call(hermit::file::kNotificationTypeKey, notificationType);
				
				FilePathValueCallback file1Path;
				inGetValueFunction.Call(hermit::file::kFirstFilePathKey, file1Path);
				FilePathValueCallback file2Path;
				inGetValueFunction.Call(hermit::file::kSecondFilePathKey, file2Path);
				std::string path1UTF8;
				if (file1Path.mFilePath != 0)
				{
					hermit::StringCallbackClassT<std::string> stringCallback;
					hermit::file::GetFilePathUTF8String(file1Path, stringCallback);
					path1UTF8 = SanitizeStringForOutput(stringCallback.mValue);
				}
				std::string path2UTF8;
				if (file2Path.mFilePath != 0)
				{
					hermit::StringCallbackClassT<std::string> stringCallback;
					hermit::file::GetFilePathUTF8String(file2Path, stringCallback);
					path2UTF8 = SanitizeStringForOutput(stringCallback.mValue);
				}
				
				//                if (inStatus == hermit::file::kCompareFilesStatus_Error)
				//                {
				//                    std::cout << "* Error: CompareFiles() failed for <" << path1UTF8 << "> and <" << path2UTF8 << ">.\n";
				//                }
				
				if (name == hermit::file::kFilesMatchNotification)
				{
					std::cout << "Match: " << path1UTF8 << "\n";
				}
				else if (notificationType.mValue == hermit::file::kFileTypesDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> are different types." << "\n";
				}
				//                else if (inStatus == hermit::file::kCompareFilesStatus_LinkTargetsMatch)
				//                {
				//                    std::cout << "Match (Link/Alias Targets Match): " << path1UTF8 << "\n";
				//                }
				else if (notificationType.mValue == hermit::file::kItemInPath1Only)
				{
					std::cout << "* File only in 1: <" << path1UTF8 << ">" << "\n";
				}
				else if (notificationType.mValue == hermit::file::kItemInPath2Only)
				{
					std::cout << "* File only in 2: <" << path2UTF8 << ">" << "\n";
				}
				else if (notificationType.mValue == hermit::file::kCreationDatesDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different creation dates." << "\n";
					if (!mSummarize)
					{
						StringValueCallback date1;
						inGetValueFunction.Call(hermit::file::kFirstDateKey, date1);
						StringValueCallback date2;
						inGetValueFunction.Call(hermit::file::kSecondDateKey, date2);
						
						std::cout << "* -- creation date 1: " << date1.mValue << "\n";
						std::cout << "* -- creation date 2: " << date2.mValue << "\n";
					}
				}
				else if (notificationType.mValue == hermit::file::kModificationDatesDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different modification dates." << "\n";
					if (!mSummarize)
					{
						StringValueCallback date1;
						inGetValueFunction.Call(hermit::file::kFirstDateKey, date1);
						StringValueCallback date2;
						inGetValueFunction.Call(hermit::file::kSecondDateKey, date2);
						
						std::cout << "* -- mod date 1: " << date1.mValue << "\n";
						std::cout << "* -- mod date 2: " << date2.mValue << "\n";
					}
				}
				else if (notificationType.mValue == hermit::file::kPackageStatesDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different package states." << "\n";
				}
				else if (notificationType.mValue == hermit::file::kFinderInfosDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different finder info flags." << "\n";
				}
				else if (notificationType.mValue == hermit::file::kXAttrsDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different xattrs." << "\n";
				}
				else if (notificationType.mValue == hermit::file::kPermissionsDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different unix permissions flags." << "\n";
				}
				else if (notificationType.mValue == hermit::file::kUserOwnersDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different user owners." << "\n";
					if (!mSummarize)
					{
						StringValueCallback owner1;
						inGetValueFunction.Call(hermit::file::kFirstOwnerKey, owner1);
						StringValueCallback owner2;
						inGetValueFunction.Call(hermit::file::kSecondOwnerKey, owner2);
						
						std::cout << "* -- user 1: " << owner1.mValue << "\n";
						std::cout << "* -- user 2: " << owner2.mValue << "\n";
					}
				}
				else if (notificationType.mValue == hermit::file::kGroupOwnersDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different group owners." << "\n";
					if (!mSummarize)
					{
						StringValueCallback owner1;
						inGetValueFunction.Call(hermit::file::kFirstOwnerKey, owner1);
						StringValueCallback owner2;
						inGetValueFunction.Call(hermit::file::kSecondOwnerKey, owner2);
						
						std::cout << "* -- group 1: " << owner1.mValue << "\n";
						std::cout << "* -- group 2: " << owner2.mValue << "\n";
					}
				}
				else if (notificationType.mValue == hermit::file::kFileSizesDiffer)
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different sizes";
					//                    std::string forkName;
					//                    if ((inInfoString1 != nullptr) && (*inInfoString1 != 0))
					//                    {
					//                        forkName = inInfoString1;
					//                        std::cout << " for fork named: " << inInfoString1;
					//                    }
					std::cout << ".\n";
					
					if (!mSummarize)
					{
						UInt64ValueCallback size1;
						inGetValueFunction.Call(hermit::file::kFirstSizeKey, size1);
						UInt64ValueCallback size2;
						inGetValueFunction.Call(hermit::file::kSecondSizeKey, size2);
						
						std::cout << "* -- size 1: " << size1.mValue << "\n";
						std::cout << "* -- size 2: " << size2.mValue << "\n";
					}
				}
				else
				{
					std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> differ.\n";
				}
				
				if (!mSummarize && (notificationType.mValue == hermit::file::kFileContentsDiffer))
				{
					UInt64ValueCallback offset;
					inGetValueFunction.Call(hermit::file::kOffsetKey, offset);
					
					std::cout << "--(offset to first difference: " << offset.mValue << ")" << "\n";
					
					uint64_t beforeRange = 128;
					if (beforeRange > offset.mValue)
					{
						beforeRange = offset.mValue;
					}
					uint64_t afterRange = 128;
					uint64_t rangeStart = offset.mValue - beforeRange;
					uint64_t rangeEnd = offset.mValue + afterRange;
					
					std::cout << ">> file 1 (" << path1UTF8 << ")" << "\n";
					DisplayByteRange(file1Path, "", rangeStart, rangeEnd);
					
					std::cout << ">> file 2 (" << path2UTF8 << ")" << "\n";
					DisplayByteRange(file2Path, "", rangeStart, rangeEnd);
				}
			}
#endif
			NOTIFY(mH_, notificationName, param);
		}
		
		//
		void PrintErrors() {
			if (!mErrors.empty()) {
				std::cout << "\n-----\nThere were errors:\n";
				
				std::vector<std::string>::const_iterator end = mErrors.end();
				for (std::vector<std::string>::const_iterator it = mErrors.begin(); it != end; ++it) {
					std::cout << *it << "\n";
				}
				
				std::cout << "-----\n\n";
			}
		}

		//
		hermit::HermitPtr mH_;
		StringVector mErrors;
		uint64_t mFirstDifferentByte;
	};
	
#if 000
	//
	//
	class CompareFilesCallback
	:
	public hermit::file::CompareFilesCallback
	{
	public:
		//
		//
		CompareFilesCallback(
							 hermit::file::FilePathPtr inFilePath1,
							 hermit::file::FilePathPtr inFilePath2)
		:
		mFilePath1(inFilePath1),
		mFilePath2(inFilePath2),
		mStatus(hermit::file::kCompareFilesStatus_Unknown),
		mFirstDifferentByte(0)
		{
		}
		
		//
		//
		bool Function(
					  const hermit::file::CompareFilesStatus& inStatus,
					  const hermit::ConstCharPtr& inInfoString1,
					  const hermit::ConstCharPtr& inInfoString2,
					  const uint64_t& inInfoUInt64)
		{
			mStatus = inStatus;
			mFirstDifferentByte = inInfoUInt64;
			
			hermit::StringCallbackClassT<std::string> stringCallback;
			hermit::file::GetFilePathUTF8String(mFilePath1.P(), stringCallback);
			std::string path1UTF8(stringCallback.mValue);
			
			hermit::file::GetFilePathUTF8String(mFilePath2.P(), stringCallback);
			std::string path2UTF8(stringCallback.mValue);
			
			if (inStatus == hermit::file::kCompareFilesStatus_Error)
			{
				std::cout << "* Error: CompareFilesWithVerify() failed for <" << path1UTF8 << "> and <" << path2UTF8 << ">.\n";
			}
			else if (inStatus == hermit::file::kCompareFilesStatus_FilesMatch)
			{
				std::cout << "Match: " << path1UTF8 << "\n";
			}
			else if (inStatus == hermit::file::kCompareFilesStatus_LinkTargetsMatch)
			{
				std::cout << "Match (Link/Alias Targets Match): " << path1UTF8 << "\n";
			}
			else if (inStatus == hermit::file::kCompareFilesStatus_CreationDatesDiffer)
			{
				std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different creation dates." << "\n";
				std::cout << "* -- creation date 1: " << inInfoString1 << "\n";
				std::cout << "* -- creation date 2: " << inInfoString2 << "\n";
			}
			else if (inStatus == hermit::file::kCompareFilesStatus_ModificationDatesDiffer)
			{
				std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> have different creation dates." << "\n";
				std::cout << "* -- mod date 1: " << inInfoString1 << "\n";
				std::cout << "* -- mod date 2: " << inInfoString2 << "\n";
			}
			else
			{
				std::cout << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> differ.\n";
			}
			
			if (inStatus == hermit::file::kCompareFilesStatus_FileContentsDiffer)
			{
				std::cout << "--(offset to first difference: " << inInfoUInt64 << ")" << "\n";
				
				uint64_t beforeRange = 128;
				if (beforeRange > inInfoUInt64)
				{
					beforeRange = inInfoUInt64;
				}
				uint64_t afterRange = 128;
				uint64_t rangeStart = inInfoUInt64 - beforeRange;
				uint64_t rangeEnd = inInfoUInt64 + afterRange;
				
				std::cout << ">> file 1 (" << path1UTF8 << ")" << "\n";
				DisplayByteRange(mFilePath1.P(), inInfoString1, rangeStart, rangeEnd);
				
				std::cout << ">> file 2 (" << path2UTF8 << ")" << "\n";
				DisplayByteRange(mFilePath2.P(), inInfoString1, rangeStart, rangeEnd);
			}
			
			return true;
		}
		
		//
		//
		hermit::file::ManagedFilePathPtr mFilePath1;
		hermit::file::ManagedFilePathPtr mFilePath2;
		hermit::file::CompareFilesStatus mStatus;
		uint64_t mFirstDifferentByte;
	};
	
	//
	//
	std::string GetFileTypeString(
								  hermit::file::FilePathPtr inPath)
	{
		std::string fileTypeString;
		hermit::file::GetFileTypeCallbackClass callback;
		hermit::file::GetFileType(
								  inPath,
								  callback);
		
		if (!callback.mSuccess)
		{
			hermit::file::LogFilePath(
									  "CompareDirectoriesCallback::Function(): GetFileType failed for: ",
									  inPath);
		}
		else if (callback.mFileType == hermit::file::kFileType_File)
		{
			fileTypeString = "Files";
		}
		else if (callback.mFileType == hermit::file::kFileType_Directory)
		{
			fileTypeString = "Directories";
		}
		else if (callback.mFileType == hermit::file::kFileType_SymbolicLink)
		{
			fileTypeString = "Links";
		}
		else
		{
			hermit::file::LogFilePath(
									  "CompareDirectoriesCallback::Function(): GetFileType returned unexpected type for: ",
									  inPath);
		}
		return fileTypeString;
	}
	
	//
	//
	class CompareDirectoriesCallback
	:
	public hermit::file::CompareDirectoriesCallback
	{
	public:
		//
		//
		CompareDirectoriesCallback()
		:
		mStatus(hermit::file::kCompareDirectoriesStatus_Unknown)
		{
		}
		
		//
		//
		bool Function(
					  const hermit::file::CompareDirectoriesStatus& inStatus,
					  const hermit::file::FilePathPtr& inPath1,
					  const hermit::file::FilePathPtr& inPath2,
					  const hermit::ConstCharPtr& inInfoString1,
					  const hermit::ConstCharPtr& inInfoString2,
					  const uint64_t& inInfoUInt64)
		{
			hermit::StringCallbackClassT<std::string> stringCallback;
			std::string path1UTF8;
			if (inPath1 != 0)
			{
				hermit::file::GetFilePathUTF8String(inPath1, stringCallback);
				path1UTF8 = stringCallback.mValue;
			}
			std::string path2UTF8;
			if (inPath2 != 0)
			{
				hermit::file::GetFilePathUTF8String(inPath2, stringCallback);
				path2UTF8 = stringCallback.mValue;
			}
			
			mStatus = inStatus;
			if (inStatus == hermit::file::kCompareDirectoriesStatus_Error)
			{
				std::cout << "* Error: " << path1UTF8 << "\n";
				
				mErrors.push_back(path1UTF8);
			}
			else if (inStatus == hermit::file::kCompareDirectoriesStatus_Match)
			{
				std::cout << "Match: " << path1UTF8 << "\n";
			}
			else
			{
				std::ostringstream output;
				std::string pathUTF8 = path1UTF8;
				if ((inStatus == hermit::file::kCompareDirectoriesStatus_FileForksDiffer) ||
					(inStatus == hermit::file::kCompareDirectoriesStatus_FileSizesDiffer) ||
					(inStatus == hermit::file::kCompareDirectoriesStatus_FileContentsDiffer))
				{
					output << "* Files <" << path1UTF8 << "> and <" << path2UTF8 << "> differ.";
					
					if (inStatus == hermit::file::kCompareDirectoriesStatus_FileContentsDiffer)
					{
						std::cout << "--(offset to first difference: " << inInfoUInt64 << ")" << "\n";
						
						uint64_t beforeRange = 128;
						if (beforeRange > inInfoUInt64)
						{
							beforeRange = inInfoUInt64;
						}
						uint64_t afterRange = 128;
						uint64_t rangeStart = inInfoUInt64 - beforeRange;
						uint64_t rangeEnd = inInfoUInt64 + afterRange;
						
						std::cout << ">> file 1 (" << path1UTF8 << ")" << "\n";
						DisplayByteRange(inPath1, inInfoString1, rangeStart, rangeEnd);
						
						std::cout << ">> file 2 (" << path2UTF8 << ")" << "\n";
						DisplayByteRange(inPath2, inInfoString1, rangeStart, rangeEnd);
					}
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_FinderInfosDiffer)
				{
					std::string fileTypeString(GetFileTypeString(inPath1));
					if (fileTypeString.empty())
					{
						return false;
					}
					output << "* " << fileTypeString << "<" << path1UTF8 << "> and <" << path2UTF8 << "> have different finder info flags set.";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_CreationDatesDiffer)
				{
					std::string fileTypeString(GetFileTypeString(inPath1));
					if (fileTypeString.empty())
					{
						return false;
					}
					output << "* " << fileTypeString << " <" << path1UTF8 << "> and <" << path2UTF8 << "> have different creation dates." << "\n";
					output << "* -- creation date 1: " << inInfoString1 << "\n";
					output << "* -- creation date 2: " << inInfoString2;
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_ModificationDatesDiffer)
				{
					std::string fileTypeString(GetFileTypeString(inPath1));
					if (fileTypeString.empty())
					{
						return false;
					}
					output << "* " << fileTypeString << " <" << path1UTF8 << "> and <" << path2UTF8 << "> have different modification dates." << "\n";
					output << "* -- mod date 1: " << inInfoString1 << "\n";
					output << "* -- mod date 2: " << inInfoString2;
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_UserOwnersDiffer)
				{
					std::string fileTypeString(GetFileTypeString(inPath1));
					if (fileTypeString.empty())
					{
						return false;
					}
					output << "* " << fileTypeString << " <" << path1UTF8 << "> and <" << path2UTF8 << "> have different user owners." << "\n";
					output << "* -- user 1: " << inInfoString1 << "\n";
					output << "* -- user 2: " << inInfoString2;
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_GroupOwnersDiffer)
				{
					std::string fileTypeString(GetFileTypeString(inPath1));
					if (fileTypeString.empty())
					{
						return false;
					}
					output << "* " << fileTypeString << " <" << path1UTF8 << "> and <" << path2UTF8 << "> have different group owners." << "\n";
					output << "* -- group 1: " << inInfoString1 << "\n";
					output << "* -- group 2: " << inInfoString2;
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_FileTypesDiffer)
				{
					output << "* Items <" << path1UTF8 << "> and <" << path2UTF8 << "> are of different types. (Perhaps one is an alias?)";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_DirectoriesDiffer)
				{
					output << "* Directories <" << path1UTF8 << "> and <" << path2UTF8 << "> differ.";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_DirectoryPackageStatesDiffer)
				{
					output << "* Directories <" << path1UTF8 << "> and <" << path2UTF8 << "> have different package states.";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_LinksDiffer)
				{
					output << "* Links <" << path1UTF8 << "> and <" << path2UTF8 << "> differ.";
					//                    output << "[ Link 1 Target: <" << link1TargetPathUTF8 << "> ] ";
					//                    output << "[ Link 2 Target: <" << link2TargetPathUTF8 << "> ]";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_FileInPath1Only)
				{
					output << "* File only in 1: <" << pathUTF8 << ">";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_FileInPath2Only)
				{
					pathUTF8 = path2UTF8;
					output << "* File only in 2: <" << pathUTF8 << ">";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_DirectoryInPath1Only)
				{
					output << "* Directory only in 1: <" << pathUTF8 << ">";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_DirectoryInPath2Only)
				{
					pathUTF8 = path2UTF8;
					output << "* Directory only in 2: <" << pathUTF8 << ">";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_LinkInPath1Only)
				{
					output << "* Link only in 1: <" << pathUTF8 << ">";
				}
				else if (inStatus == hermit::file::kCompareDirectoriesStatus_LinkInPath2Only)
				{
					pathUTF8 = path2UTF8;
					output << "* Link only in 2: <" << pathUTF8 << ">";
				}
				else
				{
					output << ">>> Unrecognized status for files <" << path1UTF8 << "> and <" << path2UTF8 <<">";
					mErrors.push_back(pathUTF8);
				}
				
				std::cout << output.str() << "\n";
				mMismatches.push_back(output.str());
			}
			return true;
		}
		
		//
		//
		hermit::file::CompareDirectoriesStatus mStatus;
		StringVector mErrors;
		StringVector mMismatches;
	};
#endif
	
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
	bool VerifyCopy(const hermit::HermitPtr& h_, hermit::file::FilePathPtr sourcePath, hermit::file::FilePathPtr destPath) {
		StringSet filenamesToSkip;
		filenamesToSkip.insert(".DS_Store");            // Finder view file which gets added/updated when you open a folder
		filenamesToSkip.insert(".ipspot_update");       // Spotlight photo data file which the OS changes on its own
		filenamesToSkip.insert("ehthumbs.db");          // Windows thumbnails file
		filenamesToSkip.insert("ehthumbs_vista.db");    // Windows thumbnails file
		filenamesToSkip.insert("Thumbs.db");            // Windows thumbnails file
		
		auto preprocessor = std::make_shared<Preprocessor>(filenamesToSkip);
		auto completion = std::make_shared<CompareCompletion>();
		hermit::file::CompareFiles(h_,
								   sourcePath,
								   destPath,
								   std::make_shared<hermit::file::HardLinkMap>(sourcePath),
								   std::make_shared<hermit::file::HardLinkMap>(destPath),
								   false,
								   false,
								   preprocessor,
								   completion);
		while (!completion->Done()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		
		//        if (!compareCallback.mMismatches.empty())
		//        {
		//            std::cout << "\n-------\nMismatch summary:\n";
		//            StringVector::const_iterator end = compareCallback.mMismatches.end();
		//            for (StringVector::const_iterator it = compareCallback.mMismatches.begin(); it != end; ++it)
		//            {
		//                std::cout << *it << "\n";
		//            }
		//        }
		//
		//        if (!compareCallback.mErrors.empty())
		//        {
		//            std::cout << "\n-------\nThere were errors:\n";
		//            StringVector::const_iterator end = compareCallback.mErrors.end();
		//            for (StringVector::const_iterator it = compareCallback.mErrors.begin(); it != end; ++it)
		//            {
		//                std::cout << *it << "\n";
		//            }
		//        }
		
		return (completion->mStatus == hermit::file::CompareFilesStatus::kSuccess);
	}
	
	//
	bool Copy(const hermit::HermitPtr& h_, hermit::file::FilePathPtr sourcePath, hermit::file::FilePathPtr destPath, bool verify) {
		auto updateCallback = std::make_shared<IntermediateUpdateCallback>();
		auto completion = std::make_shared<CopyCompletion>();
		hermit::file::FileSystemCopy(h_, sourcePath, destPath, updateCallback, completion);
		while (!completion->Done()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		
		bool success = (completion->mResult == hermit::file::FileSystemCopyResult::kSuccess);
		if (!updateCallback->mErrors.empty()) {
			std::cout << "\n-------\nThere were errors:\n";
			auto end = std::end(updateCallback->mErrors);
			for (auto it = std::begin(updateCallback->mErrors); it != end; ++it) {
				std::cout << *it << "\n";
			}
			success = false;
		}
		if (!success) {
			std::cout << "COPY FAILED." << "\n";
			return false;
		}
		
		if (verify) {
			std::cout << "Copy complete. Verifying..." << "\n";
			success = VerifyCopy(h_, sourcePath, destPath);
			if (!success) {
				std::cout << "VERIFY FAILED." << "\n";
			}
		}
		return success;
	}
	
	//
	static int Copy(const std::string& inPath1, const std::string& inPath2, bool inVerify) {
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
		hermit::string::SimplifyPath(h_, inPath1, workingDir, simplifiedPath1);
		if (simplifiedPath1.empty()) {
			NOTIFY_ERROR(h_, "SimplifyPath failed for input path:", inPath1);
			return EXIT_FAILURE;
		}
		
		std::string simplifiedPath2;
		hermit::string::SimplifyPath(h_, inPath2, workingDir, simplifiedPath2);
		if (simplifiedPath2.empty()) {
			NOTIFY_ERROR(h_, "SimplifyPath failed for input path:", inPath2);
			return EXIT_FAILURE;
		}
		
		hermit::file::FilePathPtr filePath1;
		hermit::file::CreateFilePathFromUTF8String(h_, simplifiedPath1, filePath1);
		if (filePath1 == nullptr) {
			NOTIFY_ERROR(h_, "CreateFilePathFromUTF8String failed for path:", simplifiedPath1);
			return EXIT_FAILURE;
		}
		
		hermit::file::FileExistsCallbackClass existsStatus;
		hermit::file::FileExists(h_, filePath1, existsStatus);
		if (!existsStatus.mSuccess) {
			NOTIFY_ERROR(h_, "FileExists failed for path:", filePath1);
			return EXIT_FAILURE;
		}
		if (!existsStatus.mExists) {
			std::cout << "copy: Source item doesn't exist at path: <" << simplifiedPath1 << ">\n";
			return EXIT_FAILURE;
		}
		
		hermit::file::FilePathPtr filePath2;
		hermit::file::CreateFilePathFromUTF8String(h_, simplifiedPath2, filePath2);
		if (filePath2 == nullptr) {
			NOTIFY_ERROR(h_, "CreateFilePathFromUTF8String failed for path:", simplifiedPath2);
			return EXIT_FAILURE;
		}
		
		int result = 0;
		hermit::file::FileExists(h_, filePath2, existsStatus);
		if (!existsStatus.mSuccess) {
			NOTIFY_ERROR(h_, "FileExists failed for path:", filePath2);
			return EXIT_FAILURE;
		}
		if (existsStatus.mExists) {
			bool pathIsDirectory = false;
			auto isDirectoryStatus = hermit::file::PathIsDirectory(h_, filePath2, pathIsDirectory);
			if (isDirectoryStatus != hermit::file::PathIsDirectoryStatus::kSuccess) {
				NOTIFY_ERROR(h_, "PathIsDirectory failed for path:", filePath2);
				return EXIT_FAILURE;
			}
			if (!pathIsDirectory) {
				std::cout << "copy: Destination path exists and is not a directory, aborting. <" << simplifiedPath2 << ">.\n";
				return EXIT_FAILURE;
			}
			
			std::string leaf;
			hermit::file::GetFilePathLeaf(h_, filePath1, leaf);
			
			hermit::file::FilePathPtr destPath;
			hermit::file::AppendToFilePath(h_, filePath2, leaf, destPath);
			if (destPath == nullptr) {
				NOTIFY_ERROR(h_, "AppendToFilePath failed for path:", filePath2, "leaf:", leaf);
				return EXIT_FAILURE;
			}
			
			result = Copy(h_, filePath1, destPath, inVerify);
		}
		else {
			hermit::file::FilePathPtr destParent;
			hermit::file::GetFilePathParent(h_, filePath2, destParent);
			if (destParent == nullptr) {
				NOTIFY_ERROR(h_, "GetFilePathParent failed for path:", filePath2);
				return EXIT_FAILURE;
			}
			
			hermit::file::FileExists(h_, destParent, existsStatus);
			if (!existsStatus.mSuccess) {
				NOTIFY_ERROR(h_, "FileExists failed for parent path:", destParent);
				return EXIT_FAILURE;
			}
			if (!existsStatus.mExists) {
				std::string destParentUTF8;
				hermit::file::GetFilePathUTF8String(h_, destParent, destParentUTF8);
				std::cout << "copy: Destination parent path not found: <" << destParentUTF8 << ">\n";
				return EXIT_FAILURE;
			}
			bool pathIsDirectory = false;
			auto isDirectoryStatus = hermit::file::PathIsDirectory(h_, destParent, pathIsDirectory);
			if (isDirectoryStatus != hermit::file::PathIsDirectoryStatus::kSuccess) {
				NOTIFY_ERROR(h_, "PathIsDirectory failed for parent path:", destParent);
				return EXIT_FAILURE;
			}
			if (!pathIsDirectory) {
				std::string destParentUTF8;
				hermit::file::GetFilePathUTF8String(h_, destParent, destParentUTF8);
				std::cout << "copy: Destination path parent exists but is not a directory, aborting. <" << destParentUTF8 << ">.\n";
				return EXIT_FAILURE;
			}
			
			result = Copy(h_, filePath1, filePath2, inVerify);
		}
		h_->PrintErrors();
		return result;
	}
	
	//
	static int copy(int argc, const char* argv[]) {
		std::list<std::string> args;
		for (int n = 1; n < argc; ++n) {
			args.push_back(argv[n]);
		}
		
		if (args.size() < 2) {
			usage();
			return EXIT_FAILURE;
		}
		bool gotSrcPath = false;
		std::string srcPath;
		bool getDestPath = false;
		std::string destPath;
		bool verify = false;
		bool verbose = false;
		while (!args.empty()) {
			std::string arg(args.front());
			if (arg == "-v") {
				verbose = true;
			}
			else if (arg == "-y") {
				verify = true;
			}
			else if (!gotSrcPath) {
				srcPath = arg;
				gotSrcPath = true;
			}
			else if (!getDestPath) {
				destPath = arg;
				getDestPath = true;
			}
			else {
				usage();
				return EXIT_FAILURE;
			}
			args.pop_front();
		}
		
		std::string caption("Copy took");
		if (verify) {
			caption = "Copy & verify took";
		}
		CoutReporter reporter;
		Timer t(reporter, caption);
		return Copy(srcPath, destPath, verify);
	}
	
} // namespace copy_Impl
using namespace copy_Impl;

//
int main(int argc, const char* argv[]) {
	time_t now = 0;
	time(&now);
	srand((unsigned int)now);
	
	return copy(argc, argv);
}


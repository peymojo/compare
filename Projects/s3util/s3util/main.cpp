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
#include <map>
#include <vector>
#include "Hermit/Foundation/Notification.h"
#include "Hermit/Utility/CommandLineTool.h"
#include "Hermit/Utility/OperationTimer.h"
#include "ListBucketsTool.h"

namespace s3util {

	//
    typedef std::map<std::string, hermit::utility::CommandLineToolPtr> CommandLineToolMap;

	//
	static void usage(const CommandLineToolMap& inTools) {
		std::cout << "usage:\n";
		std::cout << "\ts3util [global_options] <command> [command_options]" << "\n";
		std::cout << "\nglobal options:" << "\n";
		std::cout << "\t" << "-t   time operation" << "\n";
		std::cout << "\ncommands:" << "\n";
		
		auto end = inTools.end();
		for (auto it = inTools.begin(); it != end; ++it) {
			it->second->Usage();
		}
	}

	//
    class HermitImpl : public hermit::Hermit {
	public:
		//
		HermitImpl() {
		}
		
		//
		virtual bool ShouldAbort() override {
			return false;
		}
		
		//
		virtual void Notify(const char* name, const void* param) override {
            if (strcmp(name, hermit::kMessageNotification) == 0) {
				auto p = (hermit::MessageParams*)param;
				
				std::stringstream strm;
                if (p->severity == hermit::MessageSeverity::kWarning) {
					strm << "Warning: ";
				}
				else if (p->severity == hermit::MessageSeverity::kError) {
					strm << "ERROR: ";
					
					std::lock_guard<std::mutex> guard(mErrorsMutex);
					mErrors.push_back(p->message);
				}
				strm << p->message;
				
				OutputLine(strm.str());
			}
		}
		
		//
		void OutputLine(const std::string& line) {
			std::lock_guard<std::recursive_mutex> guard(mStreamMutex);
			std::cout << line << std::endl;
		}
		
		//
		void PrintErrors(bool clearErrors) {
			std::lock_guard<std::mutex> guard(mErrorsMutex);
			if (!mErrors.empty()) {
				OutputLine("");
				OutputLine("-----");
				OutputLine("There were errors:");
				
				for (auto it = begin(mErrors); it != end(mErrors); ++it) {
					OutputLine((*it).c_str());
				}
				
				OutputLine("-----");
				OutputLine("");
				
				if (clearErrors) {
					mErrors.clear();
				}
			}
		}
		
	private:
		std::recursive_mutex mStreamMutex;
		std::mutex mErrorsMutex;
		std::vector<std::string> mErrors;
	};

	//
	class CoutReporter {
	public:
		//
		void Report(const std::string& tag, const time_t& timeElapsed) {
			static const time_t oneMinute = 60;
			static const time_t oneHour = 60 * oneMinute;
			static const time_t oneDay = oneHour * 24;
			static const time_t oneYear = oneDay * 365;
			
			time_t t = timeElapsed;
			if (timeElapsed == 0) {
				std::cout << tag << ": less than one second." << "\n";
			}
			else if (timeElapsed == 1) {
				std::cout << tag << ": 1 second." << "\n";
			}
			else if (timeElapsed < 60) {
				std::cout << tag << ": " << timeElapsed << " seconds." << "\n";
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
				
				std::cout << tag << ": " << formattedTimeStream.str() << "(" << timeElapsed << " seconds).\n";
			}
		}
	};

	//
    typedef hermit::utility::OperationTimer<CoutReporter> Timer;

	//
	int main(std::list<std::string> args) {
		CommandLineToolMap tools;
		tools.insert(CommandLineToolMap::value_type("list_buckets", std::make_shared<ListBucketsTool>()));
		
		if (args.empty()) {
			usage(tools);
			return EXIT_FAILURE;
		}

		bool timeOperation = false;
		std::string command;
		while (!args.empty()) {
			std::string arg(args.front());
			args.pop_front();
			if (arg == "-t") {
				timeOperation = true;
			}
			else {
				command = arg;
				break;
			}
		}
		if (command.empty()) {
			usage(tools);
			return EXIT_FAILURE;
		}
		
		auto h_ = std::make_shared<HermitImpl>();
		
		CoutReporter reporter;
		Timer t(reporter, "s3util took");
		
		int result = EXIT_FAILURE;
		CommandLineToolMap::iterator toolIt = tools.find(command);
		if (toolIt != tools.end()) {
			result = toolIt->second->Run(h_, args);
		}
		else {
			std::cout << "s3util: unrecognized command: " << command << "\n";
			usage(tools);
			result = EXIT_FAILURE;
		}
		
		if (!timeOperation || (result != 0)) {
			t.SuppressOutput();
		}
		return result;
	}
	
} // namespace s3util

//
int main(int argc, const char * argv[]) {
	std::list<std::string> args;
	for (int n = 1; n < argc; ++n) {
		args.push_back(argv[n]);
	}
    return s3util::main(args);
}

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

#ifndef ListBucketsTool_h
#define ListBucketsTool_h

#include "Hermit/Utility/CommandLineTool.h"

namespace s3util {
	
	//
    class ListBucketsTool : public hermit::utility::CommandLineTool {
		//
		virtual void Usage() const;
		
		//
        virtual int Run(const hermit::HermitPtr& h_, const std::list<std::string>& args);
	};
	
} // namespace s3util

#endif /* ListBucketsTool_h */

//------------------------------------------------------------------------------
// Mongoose/Tests/Mongoose_Test_Memory_exe.cpp
//------------------------------------------------------------------------------

// Mongoose Graph Partitioning Library, Copyright (C) 2017-2018,
// Scott P. Kolodziej, Nuri S. Yeralan, Timothy A. Davis, William W. Hager
// Mongoose is licensed under Version 3 of the GNU General Public License.
// Mongoose is also available under other licenses; contact authors for details.
// SPDX-License-Identifier: GPL-3.0-only

//------------------------------------------------------------------------------

#include "Mongoose.hpp"
#include "Mongoose_Test.hpp"

using namespace Mongoose;

#undef LOG_ERROR
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_TEST
#define LOG_ERROR 1
#define LOG_WARN 1
#define LOG_INFO 0
#define LOG_TEST 1

int main(int argn, const char **argv)
{

    if ((major_version ( ) != Mongoose_VERSION_MAJOR) ||
        (minor_version ( ) != Mongoose_VERSION_MINOR) ||
        (patch_version ( ) != Mongoose_VERSION_PATCH))
    {
        return EXIT_FAILURE;
    }

    SuiteSparse_start();

    if (argn != 2)
    {
        // Wrong number of arguments - return error
        SuiteSparse_finish();
        return EXIT_FAILURE;
    }

    // Read in input file name
    std::string inputFile = std::string(argv[1]);

    // Set Logger to report only Test-level messages
    Logger::setDebugLevel(Test);

    // Run the memory test
    int status = runMemoryTest(inputFile);

    SuiteSparse_finish();
    
    return status;
}

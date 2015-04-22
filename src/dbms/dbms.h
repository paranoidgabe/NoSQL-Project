#ifndef DBMS_H_
#define DBMS_H_

#include <rapidjson/document.h>
#include <vector>
#include "../parsing/Parser.h"
#include "../mmap_filesystem/Filesystem.h"
#include "../storage/HerpHash.h"

#define LENGTH(A) sizeof(A)/sizeof(A[0])
#ifndef UNUSED
#define UNUSED(id)
#endif

#define MAJOR_VERSION 0
#define MINOR_VERSION 1

//void execute(Parsing::Query &, Storage::LinearHash<std::string> &);

//typedef Storage::LinearHash<uint64_t> INDICES;
//typedef Storage::LinearHash<std::string> META;
typedef Storage::HerpHash<std::string,std::vector<std::string>> META;
//typedef std::map<std::string,std::string> META;
typedef Storage::Filesystem FILESYSTEM;

std::string SpecialValueComparisons[] = { "#gt", "#lt", "#eq", "#contains", "#starts", "#ends" };
std::string SpecialKeyComparisons[] = { "#exists", "#isnull", "#isstr", "#isnum", "#isbool", "#isarray", "#isobj" };

#endif

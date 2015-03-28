#ifndef _PARSER_H_
#define _PARSER_H_

#include <string>
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "Scanner.h"

// Convert a JSON object to a std::string
inline std::string docToString(rapidjson::Document *doc) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc->Accept(writer);
    std::string str = buffer.GetString();
    return str;
}

namespace Parsing {
	const std::string Aggregates[] = {"AVG", "MIN", "MAX", "SUM", "STDEV" /*, TODO: Others. */};
	const std::string Commands[] = {"CREATE", "INSERT", "SELECT", "DELETE", "UPDATE", "SHOW" /*, TODO: Others. */};
	const std::string CreateArgs[] = {"PROJECT", "DOCUMENT"};
	const std::string SelectArgs[] = {"WHERE", "GROUP BY"};
	enum Command {
		CREATE = 0,
		INSERT = 1,
		SELECT = 2,
		DELETE = 3,
		UPDATE = 4,
		SHOW   = 5
	};

	enum Aggregate {
		AVG   = 0,
		MIN   = 1,
		MAX   = 2,
		SUM   = 3,
		STDEV = 4
	};

	struct Query {
		Command command;
		std::string *project;
		rapidjson::Document *with;
		rapidjson::Document *where;
		rapidjson::Document *fields;
		int limit;
		Query(): project(NULL), with(NULL), where(NULL), fields(NULL), limit(-1) {}
		void print() {
			std::cout << "Command: " << Commands[command] << std::endl;
			if (project) {
				std::cout << "Project: " << *project << std::endl;
			}
			if (with) {
				std::cout << "With: " << docToString(with) << std::endl;
			}
			if (where) {
				std::cout << "Where: " << docToString(where) << std::endl;
			}
			if (fields) {
				std::cout << "Fields: " << docToString(fields) << std::endl;
			}
			if (limit > -1) {
				std::cout << "Limit: " << limit << std::endl;
			}
		}
	};

	class Parser {
	public:
		Parser(std::string q): sc(q) {}
		Parsing::Query* parse();
	private:
		Scanner sc;
		bool insert(Query &);
		bool update(Query &);
		bool select(Query &);
		bool ddelete(Query &);
		bool create(Query &);
		bool show(Query &q);
		bool aggregatePending();
		bool aggregate(rapidjson::Document *);
		bool limitPending();
		rapidjson::Document *fieldList();
	};
}

#endif

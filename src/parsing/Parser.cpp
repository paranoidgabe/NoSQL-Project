/*
Query parser for the NoSQL database management system.
*/

#include <iostream>
#include <string>
#include "Scanner.h"
#include "Parser.h"

std::string toLower(std::string s) {
	std::string ss = std::string(s);
	std::transform(ss.begin(), ss.end(), ss.begin(), ::tolower);
	return ss;
}

Parsing::Query* Parsing::Parser::parse() {
	std::string token = toLower(Parsing::Parser::sc.nextToken());
	Parsing::Query *q = new Parsing::Query();
	q->aggregate = NONE;
	bool result = false;
	if (!token.compare("create")) {
		result = create(*q);
	} else if (!token.compare("insert")) {
		result = insert(*q);
	} else if (!token.compare("append")) {
		result = append(*q);
	} else if (!token.compare("remove")) {
		result = remove(*q);
	} else if (!token.compare("select")) {
		result = select(*q);
	} else if (!token.compare("delete")) {
		result = ddelete(*q);
	}

	if (!result) {
		delete q;
		return NULL;
	}

	try {
		char t = Parsing::Parser::sc.nextChar();
		if (t != ';') {
			std::cout << "PARSING ERROR: Expected semicolon but found '" << t << "'" << std::endl;
			delete q;
			return NULL;
		}	
	} catch (std::runtime_error &e) {
		std::cout << "PARSING ERROR: End of query reached.  Expected a semicolon." << std::endl;
		delete q;
		return NULL;
	}

	return q;
}

bool Parsing::Parser::insert(Parsing::Query &q) {
	q.command = INSERT;
	std::string token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("into")) {
		std::cout << "PARSING ERROR: Expected 'into', found " << token << std::endl;
		return false;
	}
	q.project = Parsing::Parser::sc.nextToken();
	if (Parsing::Parser::sc.nextChar() != '.') {
		std::cout << "PARSING ERROR: Expected a dot" << std::endl;
		return false;
	}
	q.documents = new List(Parsing::Parser::sc.nextToken());
	q.value = Parsing::Parser::sc.nextJSON();
	return true;
}

bool Parsing::Parser::append(Parsing::Query &q) {
	q.command = APPEND;
	std::string token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("to")) {
		std::cout << "PARSING ERROR: Expected 'to', found " << token << std::endl;
		return false;
	}
	q.project = Parsing::Parser::sc.nextToken();
	if (Parsing::Parser::sc.nextChar() != '.') {
		std::cout << "PARSING ERROR: Expected a dot" << std::endl;
		return false;
	}
	q.documents = new List(Parsing::Parser::sc.nextToken());
	q.value = Parsing::Parser::sc.nextJSON();
	return true;
}

bool Parsing::Parser::remove(Parsing::Query &q) {
	q.command = REMOVE;
	std::string token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("from")) {
		std::cout << "PARSING ERROR: Expected 'from', found " << token << std::endl;
		return false;
	}
	q.project = Parsing::Parser::sc.nextToken();
	if (Parsing::Parser::sc.nextChar() != '.') {
		std::cout << "PARSING ERROR: Expected a dot" << std::endl;
		return false;
	}
	q.documents = new List(Parsing::Parser::sc.nextToken());
	token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("where")) {
		std::cout << "PARSING ERROR: Expected 'where', found " << token << std::endl;
		return false;
	}
	token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("key")) {
		std::cout << "PARSING ERROR: Expected 'key', found " << token << std::endl;
		return false;
	}
	if (Parsing::Parser::sc.nextChar() != '=') {
		std::cout << "PARSING ERROR: Expected an equals" << std::endl;
		return false;
	}
	q.key = Parsing::Parser::sc.nextString();
	if (andValuePending()) {
		q.value = Parsing::Parser::sc.nextJSON();
	}
	return true;
}

bool Parsing::Parser::select(Parsing::Query &q) {
	q.command = SELECT;
	if (aggregatePending()) {
		aggregate(q);
	}
	std::string token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("from")) {
		std::cout << "PARSING ERROR: Expected 'from', found " << token << std::endl;
		return false;
	}
	q.project = Parsing::Parser::sc.nextToken();
	if (Parsing::Parser::sc.nextChar() != '.') {
		std::cout << "PARSING ERROR: Expected a dot" << std::endl;
		return false;
	}
	q.documents = new List(Parsing::Parser::sc.nextToken());
	if (wherePending()) {
		return where(q);
	}
	return true;
}

bool Parsing::Parser::ddelete(Parsing::Query &q) {
	q.command = DELETE;
	std::string token = Parsing::Parser::sc.nextToken();
	if (!toLower(token).compare("document")) {
		q.project = Parsing::Parser::sc.nextToken();
		if (Parsing::Parser::sc.nextChar() != '.') {
			std::cout << "PARSING ERROR: Expected a dot" << std::endl;
			return false;
		}
		q.documents = new List(Parsing::Parser::sc.nextToken());
		return true;
	} else if (!toLower(token).compare("project")) {
		q.project = Parsing::Parser::sc.nextToken();
		return true;
	} else {
		std::cout << "PARSING ERROR: Expected 'document' or 'project'." << std::endl;
		return false;
	}

	return false;
}

bool Parsing::Parser::create(Parsing::Query &q) {
	q.command = CREATE;
	std::string token = toLower(Parsing::Parser::sc.nextToken());
	if (!token.compare("project")) {
		q.project = Parsing::Parser::sc.nextToken();
		if (withDocumentsPending()) {
			if (Parsing::Parser::sc.nextChar() == '(') {
				try {
					q.documents = idList();
				} catch (std::runtime_error &e) {
					std::cout << e.what() << std::endl;
					return false;
				}
			} else {
				std::cout << "PARSING ERROR: Expected open paren." << std::endl;
				return false;
			}
		}
	} else if (!token.compare("document")) {
		q.project = Parsing::Parser::sc.nextToken();
		if (Parsing::Parser::sc.nextChar() != '.') {
			std::cout << "PARSING ERROR: Expected a dot." << std::endl;
			return false;
		}
		q.documents = new List(Parsing::Parser::sc.nextToken());
		if (withValuePending()) {
			q.value = Parsing::Parser::sc.nextJSON();
		}
	} else {
		std::cout << "PARSING ERROR: Expected 'document' or 'project'." << std::endl;
		return false;
	}
	return true;

}

bool Parsing::Parser::where(Parsing::Query &q) {
	std::string token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("key")) {
		std::cout << "PARSING ERROR: Expected 'key', found " << token << std::endl;
		return false;
	}	
	if (Parsing::Parser::sc.nextChar() == '=') {
		q.key = Parsing::Parser::sc.nextString();
		return true;
	} else {
		Parsing::Parser::sc.push_back(1);
	}
	token = Parsing::Parser::sc.nextToken();
	if (toLower(token).compare("in")) {
		std::cout << "PARSING ERROR: Expected 'in', found " << token << std::endl;
		return false;
	}
	q.key = Parsing::Parser::sc.nextString();
	return true;
}

void Parsing::Parser::aggregate(Parsing::Query &q) {
	std::string token = Parsing::Parser::sc.nextToken();
	int numAggregates = sizeof(Aggregates) / sizeof(std::string);
	for (int i=0; i<numAggregates; ++i) {
		if (!toLower(token).compare(toLower(Aggregates[i]))) {
			q.aggregate = (Aggregate)i;
		}
	}
}

Parsing::List * Parsing::Parser::idList() {
	std::string id = Parsing::Parser::sc.nextToken();
	if (id.size() == 0) {
		std::cout << "PARSING ERROR: Expected identifier." << std::endl;
		return NULL;
	}
	List *doc = new List(id);
	char next = Parsing::Parser::sc.nextChar();
	if (next == ',') {
		doc->next = idList();
	} else if (next == ')') {
		return doc;
	} else {
		throw std::runtime_error("PARSING ERROR: Expected closed paren.");
	}
	return doc;
}

bool Parsing::Parser::withDocumentsPending() {
	std::string with = Parsing::Parser::sc.nextToken();
	std::string documents = Parsing::Parser::sc.nextToken();
	bool found = false;
	if (!toLower(with).compare("with") && !toLower(documents).compare("documents")) {
		found = true;
	} else {
		Parsing::Parser::sc.push_back(documents);
		Parsing::Parser::sc.push_back(with);
	}
	return found;
}

bool Parsing::Parser::withValuePending() {
	std::string with = Parsing::Parser::sc.nextToken();
	std::string value = Parsing::Parser::sc.nextToken();
	bool found = false;
	if (!toLower(with).compare("with") && !toLower(value).compare("value")) {
		found = true;
	} else {
		Parsing::Parser::sc.push_back(value);
		Parsing::Parser::sc.push_back(with);
	}
	return found;
}

bool Parsing::Parser::wherePending() {
	bool result = false;
	std::string token = Parsing::Parser::sc.nextToken();
	if (!toLower(token).compare("where")) {
		result = true;
	} else {
		Parsing::Parser::sc.push_back(token);
	}
	return result;
}

bool Parsing::Parser::andValuePending() {
	bool result = false;
	std::string aand = Parsing::Parser::sc.nextToken();
	std::string value = Parsing::Parser::sc.nextToken();
	if (!toLower(aand).compare("and") && !toLower(value).compare("value") && Parsing::Parser::sc.nextChar() == '=') {
		result = true;
	} else {
		Parsing::Parser::sc.push_back(aand);
		Parsing::Parser::sc.push_back(value);
	}
	return result;
}

bool Parsing::Parser::aggregatePending() {
	std::string token = Parsing::Parser::sc.nextToken();
	bool result = false;
	int numAggregates = sizeof(Aggregates) / sizeof(std::string);
	for (int i=0; i<numAggregates; ++i) {
		if (!toLower(token).compare(toLower(Aggregates[i]))) {
			result = true;
		}
	}
	Parsing::Parser::sc.push_back(token);
	return result;
}

/*
int main(int argc, char **argv) {
	Parsing::Parser p("select from *.*;");
	Parsing::Query *q = p.parse();
	q->print();
	return 0;
}
*/
//============================================================================
// Name        : feedhandler.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "feedhandler.hpp"
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char **argv) {

	if (argc != 2)
	{
		std::cout << "Must supply filename" << std::endl;
		return 1;
	}

	ifstream infile;
	infile.open(argv[1]);
	if (!infile.is_open())
	{
		std::cout << "Cannot open file " << argv[1] << std::endl;
		return 1;
	}
	std::cout << "Successfully opened file " << argv[1] << std::endl;

	feedhandler fh(1);
	std::string line;
	while (!infile.eof() && !infile.bad())
	{
		getline(infile, line);
		fh.process_message(line);
	}

	infile.close();

	fh.print_stats();

	return 0;
}

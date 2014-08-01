
#ifndef _FEEDHANDLER_H_
#define _FEEDHANDLER_H_

#include "orderbook.hpp"
#include <iostream>

class feedhandler
{
public:
	//initialise with how often to print the orderbook and the stream to write it to
	feedhandler(int ob_print_frequency, std::ostream &os);

	//print stats on the feed we've been processing
	void print_stats() const;

	//process the message
	void process_message(const std::string &line);

private:
	void record_failure();

private: //state
	const int ob_print_frequency_;
	std::ostream &os_;

	orderbook ob_;
	int parse_failure_count_ = 0;
	int messages_processed_ = 0;
};

#endif

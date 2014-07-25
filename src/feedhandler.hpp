
#ifndef _FEEDHANDLER_H_
#define _FEEDHANDLER_H_

#include "orderbook.hpp"
#include <iostream>

class feedhandler
{
public:
	explicit feedhandler(int ob_print_frequency)
		: ob_print_frequency_(ob_print_frequency)
	{

	}

	enum class parse_state
	{
		type,
		order_id,
		side,
		price,
		size,
		done
	};
	enum class action
	{
		add,
		modify,
		remove,
		trade,
		none
	};

	void print_stats()
	{
		const auto &ob_stats = ob_.get_error_stats();
		std::cout << "ERROR STATS:" << std::endl;
		std::cout << "  unparseable: " << parse_failure_count_ << std::endl;
		std::cout << "  crossed book with no trades: " << ob_stats.crossed_book_no_trades << std::endl;
		std::cout << "  duplicate order ids: " << ob_stats.duplicate_order_ids << std::endl;
		std::cout << "  invalid inputs: " << ob_stats.invalid_inputs << std::endl;
		std::cout << "  modifies without order: " << ob_stats.modifies_without_order << std::endl;
		std::cout << "  removes without order: " << ob_stats.removes_without_order << std::endl;
		std::cout << "  trades without order: " << ob_stats.trade_without_order << std::endl;
	}

	void process_message(const std::string &line)
	{
		std::cout << line << ": ";

		char type, s;
		int order_id, volume;
		double price;
		char dummy;
		//trade
		if (sscanf(line.c_str(), "%c,%d,%lf%c", &type, &volume, &price, &dummy) == 3)
		{
			if (type != 'T')
			{
				record_failure();
				return;
			}
			ob_.on_trade(price, volume);

			//output the trade stats every message
			const auto &trade_stats = ob_.get_current_trade_stats();
			std::cout << trade_stats.cumulative_trade_volume << "@" << trade_stats.last_trade_price << std::endl;
		}
		//order action
		else if (sscanf(line.c_str(), "%c,%d,%c,%d,%lf%c", &type, &order_id, &s, &volume, &price, &dummy) == 5)
		{
			side action_side;
			if (s == 'B') action_side = side::bid;
			else if (s == 'S') action_side = side::ask;
			else
			{
				record_failure();
				return;
			}

			switch(type)
			{
			case 'A': ob_.on_order_add(action_side, order_id, price, volume); break;
			case 'M': ob_.on_order_modify(action_side, order_id, price, volume); break;
			case 'X': ob_.on_order_remove(action_side, order_id); break;
			default: record_failure(); return;
			}

			//print out the midpoint
			const auto midpoint = ob_.get_midpoint();
			if (midpoint == 0)
			{
				std::cout << "NAN" << std::endl;
			}
			else
			{
				std::cout << midpoint << std::endl;
			}
		}
		else
		{
			record_failure();
			return;
		}

		std::cout << std::endl;

		++messages_processed_;
		if (ob_print_frequency_ != 0 && messages_processed_ == ob_print_frequency_)
		{
			//print the ob
			std::cout << std::endl << "Current Orderbook:" << std::endl;
			ob_.print_ob(std::cout);
			std::cout << std::endl;
			messages_processed_ = 0;
		}
	}

//		unsigned start = 0;
//		parse_state state = parse_state::type;
//		action action_type = action::none;
//
//		auto end = line.find(',');
//		while (end != std::string::npos)
//		{
//			switch(state)
//			{
//			case parse_state::type:
//			{
//				if (end - start != 1)
//				{
//					record_failure();
//					return;
//				}
//				const char type = *start;
//				switch(type)
//				{
//				case 'A': action_type = action::add; state = parse_state::order_id; break;
//				case 'M': action_type = action::modify; state = parse_state::order_id; break;
//				case 'X': action_type = action::remove; state = parse_state::order_id; break;
//				case 'T': action_type = action::trade; state = parse_state::price; break;
//				default: record_failure(); return;
//				}
//				break;
//			}
//			case parse_state::order_id:
//			{
//
//			}
//
//			case parse_state::done
//				record_failure();
//				return;
//			}
//		}
//		if (state != parse_state::done)
//		{
//			record_failure();
//			return;
//		}
//	}


private:
	bool str_to_int(const char *buf, size_t len, int &)        // digits only
	{
	        int n=0;

	        while (len--)
	                n = n*10 + *buf++ - '0';

	        return n;
	}

	void record_failure()
	{
		std::cout << " UNPARSABLE" << std::endl;
		++parse_failure_count_;
	}

	const int ob_print_frequency_;

	orderbook ob_;
	int parse_failure_count_ = 0;
	int messages_processed_ = 0;
};

#endif

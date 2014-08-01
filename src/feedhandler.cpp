#include "feedhandler.hpp"

namespace
{
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

	bool str_to_int(const char *buf, size_t len, int &)        // digits only
	{
		int n=0;

		while (len--)
		{
			n = n*10 + *buf++ - '0';
		}

		return n;
	}

}

feedhandler::feedhandler(int ob_print_frequency, std::ostream &os)
		: ob_print_frequency_(ob_print_frequency),
		  os_(os)
	{

	}

void feedhandler::print_stats() const
{
	const auto &ob_stats = ob_.get_error_stats();
	os_ << std::endl;
	os_ << "ERROR STATS:" << std::endl;
	os_ << "  unparseable: " << parse_failure_count_ << std::endl;
	os_ << "  crossed book with no trades: " << ob_stats.crossed_book_no_trades << std::endl;
	os_ << "  duplicate order ids: " << ob_stats.duplicate_order_ids << std::endl;
	os_ << "  invalid inputs: " << ob_stats.invalid_inputs << std::endl;
	os_ << "  modifies without order: " << ob_stats.modifies_without_order << std::endl;
	os_ << "  removes without order: " << ob_stats.removes_without_order << std::endl;
	os_ << "  trades without order: " << ob_stats.trade_without_order << std::endl;
	os_ << std::endl;
}

void feedhandler::record_failure()
{
	os_ << " UNPARSABLE" << std::endl;
	++parse_failure_count_;
}

void feedhandler::process_message(const std::string &line)
{
	os_ << line << ": ";

	//pretty basic scanning of the line; attempt to sscanf the various
	// types of action that are available; if we can't match anything then
	// it means something was wrong with the line and we mark it unparsable

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
		os_ << trade_stats.cumulative_trade_volume << "@" << trade_stats.last_trade_price << std::endl;
	}
	//order action
	else if (sscanf(line.c_str(), "%c,%d,%c,%d,%lf%c", &type, &order_id, &s, &volume, &price, &dummy) == 5)
	{
		side action_side;
		if (s == 'B')
		{
			action_side = side::bid;
		}
		else if (s == 'S')
		{
			action_side = side::ask;
		}
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
			os_ << "NAN" << std::endl;
		}
		else
		{
			os_ << midpoint << std::endl;
		}
	}
	else
	{
		record_failure();
		return;
	}

	++messages_processed_;
	if (ob_print_frequency_ != 0 && messages_processed_ == ob_print_frequency_)
	{
		//print the ob
		os_ << std::endl << std::endl << "Current Orderbook:" << std::endl;
		ob_.print_ob(os_);
		os_ << std::endl;
		messages_processed_ = 0;
	}
}

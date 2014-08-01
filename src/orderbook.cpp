/*
 * orderbook.cpp
 *
 *  Created on: 31 Jul 2014
 *      Author: ger
 */
#include "orderbook.hpp"

void orderbook::print_ob(std::ostream &os) const
{
	//march through the two sides, printing each of their details in descending price order
	auto ask_iter = rbegin(side::ask);
	auto bid_iter = begin(side::bid);

	const auto end_ask = rend(side::ask);
	const auto end_bid = end(side::bid);
	double curr_price = 0;
	while (ask_iter != end_ask && bid_iter != end_bid)
	{
		const auto ask_price = ask_iter->first;
		const auto bid_price = bid_iter->first;
		if (ask_price >= bid_price)
		{
			if (ask_price != curr_price)
			{
				curr_price = ask_price;
				os << std::endl;
				os << curr_price;
			}
			os << " S " << ask_iter->second;
			++ask_iter;
		}
		else
		{
			if (bid_price != curr_price)
			{
				curr_price = bid_price;
				os << std::endl;
				os << curr_price;
			}
			os << " B " << bid_iter->second;
			++bid_iter;
		}
	}
	while (ask_iter != end_ask)
	{
		const auto ask_price = ask_iter->first;
		if (ask_price != curr_price)
		{
			curr_price = ask_price;
			os << std::endl;
			os << curr_price;
		}
		os << " S " << ask_iter->second;
		++ask_iter;
	}
	while (bid_iter != end_bid)
	{
		const auto bid_price = bid_iter->first;
		if (bid_price != curr_price)
		{
			curr_price = bid_price;
			os << std::endl;
			os << curr_price;
		}
		os << " B " << bid_iter->second;
		++bid_iter;
	}
	os << std::endl;
}

const orderbook::ordered_price_to_volumes::value_type *orderbook::get_order_in_position(side s, unsigned position) const
{
	if (side_to_price_to_vols_[(int)s].size() <= position)
	{
		return nullptr;
	}

	auto iter = side_to_price_to_vols_[(int)s].begin();
	for (unsigned i = 0; i < position; ++i) ++iter;
	return &(*iter);
}

int orderbook::get_volume(side s, double price) const
{
	const auto orders_at_price = side_to_price_to_vols_[(int)s].equal_range(price);
	return std::accumulate(orders_at_price.first, orders_at_price.second,
			0,
			[](int accumulated, const ordered_price_to_volumes::value_type &elem)
			{
				return accumulated + elem.second;
			});
}




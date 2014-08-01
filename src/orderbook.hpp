
#ifndef __ORDERBOOK_H__
#define __ORDERBOOK_H__

#include "enums.hpp"

#include <unordered_map>
#include <map>
#include <vector>
#include <list>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <iostream>

inline bool order_ascending(double left, double right)
{
	return left < right;
}

inline bool order_descending(double left, double right)
{
	return right < left;
}

class orderbook
{
public:
	orderbook()
		: best_prices_(2)
	{
		//bids are ordered from highest to lowest
		side_to_price_to_vols_.emplace_back(order_descending);

		//asks are ordered lowest to highest
		side_to_price_to_vols_.emplace_back(order_ascending);
	}

	~orderbook() = default;

	//in C++11 multimap guarantees stable ordering of elements with duplicate keys
	typedef std::function<bool(double, double)> comparator;
	using ordered_price_to_volumes = std::multimap<double, int, comparator>;

	//iterators to each side's levels
	ordered_price_to_volumes::const_iterator begin(side s) const { return side_to_price_to_vols_[(int)s].begin(); }
	ordered_price_to_volumes::const_iterator end(side s) const { return side_to_price_to_vols_[(int)s].end(); }

	ordered_price_to_volumes::const_reverse_iterator rbegin(side s) const { return side_to_price_to_vols_[(int)s].rbegin(); }
	ordered_price_to_volumes::const_reverse_iterator rend(side s) const { return side_to_price_to_vols_[(int)s].rend(); }

	//print the orderbook to the given stream
	void print_ob(std::ostream &os) const;

	//check if the book is crossed
	bool is_crossed() const { return best_prices_[(int)side::bid] >= best_prices_[(int)side::ask]; }

	//get the calculated midpoint
	double get_midpoint() const	{ return midpoint_; }

	//retrieve the order on the given side/in the given position
	//returns null if that position does not exist
	const ordered_price_to_volumes::value_type *get_order_in_position(side s, unsigned position) const;

	//get the number of orders on the given side
	int get_order_count_on_side(side s) const { return side_to_price_to_vols_[(int)s].size(); }

	//get the best price on the given side
	double get_best_price(side s) const	{ return best_prices_[(int)s]; }

	//get the total volume at the given price point
	int get_volume(side s, double price) const;

	//the trade stats that we've seen
	struct trade_stats
	{
		double last_trade_price = 0.0;
		uint64_t cumulative_trade_volume = 0;
	};
	const trade_stats &get_current_trade_stats() const
	{
		return trade_stats_;
	}

	//the error stats that we've seen
	struct error_stats
	{
		int duplicate_order_ids = 0;
		int trade_without_order = 0;
		int removes_without_order = 0;
		int modifies_without_order = 0;
		int crossed_book_no_trades = 0;
		int invalid_inputs = 0;
	};
	const error_stats &get_error_stats() const
	{
		return error_stats_;
	}

	//trade message seen; update the error/trade stats, won't affect the book
	//returns false if any issue detected
	bool on_trade(double price, int volume)
	{
		if (!is_crossed())
		{
			++error_stats_.trade_without_order;
			return false;
		}

		//trade isn't valid if it's greater than the max bid or less than the min ask
		if (price > best_prices_[(int)side::bid] || price < best_prices_[(int)side::ask])
		{
			++error_stats_.trade_without_order;
			return false;
		}

		if (price == trade_stats_.last_trade_price)
		{
			trade_stats_.cumulative_trade_volume += volume;
		}
		else
		{
			trade_stats_.last_trade_price = price;
			trade_stats_.cumulative_trade_volume = volume;
		}
		return true;
	}

	//apply the given modification to the order id
	//returns false if any issues found, in which case nothing will have been applied
	//if the volume has gone to zero the order will be removed
	bool on_order_modify(side s, int order_id, double price, int volume)
	{
		if (!check_validity(order_id, price, volume))
		{
			return false;
		}

		//if it doesn't exist something is wrong
		auto result = order_id_to_details_.find(order_id);
		if (result == order_id_to_details_.end())
		{
			++error_stats_.modifies_without_order;
			return false;
		}

		//side isn't right, something is wrong
		std::pair<side, ordered_price_to_volumes::iterator> &side_and_iter = result->second;
		if (side_and_iter.first != s)
		{
			++error_stats_.modifies_without_order;
			return false;
		}

		//if the new volume is zero this is actually a remove instead
		if (volume == 0)
		{
			side_to_price_to_vols_[(int)side_and_iter.first].erase(side_and_iter.second);

			//and now remove the order
			order_id_to_details_.erase(result);

			update_best_prices(s);
		}
		//if the price didn't change we can just update the volume
		else if (side_and_iter.second->first == price)
		{
			side_and_iter.second->second = volume;
		}
		//but if it did change we have to remove, re-insert and re-calculate best bid/offer
		else
		{
			side_to_price_to_vols_[(int)s].erase(side_and_iter.second);
			side_and_iter.second = side_to_price_to_vols_[(int)s].emplace(price, volume);
			update_best_prices(s);
		}
		return true;
	}

	//remove the given order id from the book
	//returns false if any issue detected
	bool on_order_remove(side s, int order_id)
	{
		if (!check_validity(order_id))
		{
			return false;
		}

		//if it doesn't exist something is wrong
		auto result = order_id_to_details_.find(order_id);
		if (result == order_id_to_details_.end())
		{
			++error_stats_.removes_without_order;
			return false;
		}

		//remove the price
		const std::pair<side, ordered_price_to_volumes::iterator> &side_and_iter = result->second;
		if (side_and_iter.first != s)
		{
			++error_stats_.removes_without_order;
			return false;
		}
		side_to_price_to_vols_[(int)side_and_iter.first].erase(side_and_iter.second);

		//and now remove the order
		order_id_to_details_.erase(result);

		update_best_prices(s);
		return true;
	}

	//add the given details to the book
	//returns false if any issues found, e.g. duplicate or conflicting data
	bool on_order_add(side s, int order_id, double price, int volume)
	{
		if (!check_validity(order_id, price, volume))
		{
			return false;
		}

		//if it exists already then something's wrong
		auto result = order_id_to_details_.insert(std::make_pair(order_id, std::make_pair(s, ordered_price_to_volumes::iterator())));
		if (!result.second)
		{
			++error_stats_.duplicate_order_ids;
			return false;
		}

		//we're good to add it to the side map and link them up
		result.first->second.second = side_to_price_to_vols_[(int)s].emplace(price, volume);

		update_best_prices(s);
		return true;
	}


private: //methods
	//something has modified our book, update the best price for that side and do the midpoint as well
	void update_best_prices(side s)
	{
		if (side_to_price_to_vols_[(int)s].empty())
		{
			best_prices_[(int)s] = 0;
		}
		else
		{
			best_prices_[(int)s] = side_to_price_to_vols_[(int)s].begin()->first;
		}
		update_midpoint();
	}

	//update the midpoint every time a price changes
	void update_midpoint()
	{
		//assume that midpoint is not valid if the book is crossed
		if (is_crossed())
		{
			midpoint_ = 0;
			return;
		}

		//if either side is zero the midpoint is also zero
		if (best_prices_[(int)side::ask] == 0 || best_prices_[(int)side::bid] == 0)
		{
			midpoint_ = 0;
			return;
		}

		//ok do the calculation
		const double diff = best_prices_[(int)side::ask] - best_prices_[(int)side::bid];
		midpoint_ = best_prices_[(int)side::bid] + (diff * 0.5);
	}

	//check the validity of an incoming order event
	bool check_validity(int order_id, double price, int size)
	{
		if (order_id < 0 || price < 0 || size < 0)
		{
			++error_stats_.invalid_inputs;
			return false;
		}
		return true;
	}

	bool check_validity(int order_id)
	{
		if (order_id < 0)
		{
			++error_stats_.invalid_inputs;
			return false;
		}
		return true;
	}

private: //state
	error_stats error_stats_;
	trade_stats trade_stats_;

	//2-element vector (one per side) containing the price-to-volumes mappings
	std::vector<ordered_price_to_volumes> side_to_price_to_vols_;

	//mapping of the order id to where the order details can be found in the price-to-volumes mappings
	std::unordered_map<int, std::pair<side, ordered_price_to_volumes::iterator>> order_id_to_details_;

	//2-element vector (one per side) containing the current best prices
	std::vector<double> best_prices_;

	//the current midpoint, updated every time a touch price changes.
	double midpoint_ = 0.0;
};

#endif



#ifndef __ORDERBOOK_H__
#define __ORDERBOOK_H__

#include "enums.hpp"

#include <map>
#include <vector>
#include <list>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <iostream>

struct order_details
{
	order_details(double p, int v) : price(p), volume(v) {}
	double price = 0.0;
	int volume = 0;
};

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

	bool is_crossed() const
	{
		return best_prices_[(int)side::bid] >= best_prices_[(int)side::ask];
	}

	double get_midpoint() const
	{
		return midpoint_;
	}

	const std::pair<const double, int> *get_order_in_position(side s, unsigned position)
	{
		if (side_to_price_to_vols_[(int)s].size() <= position)
		{
			return nullptr;
		}

		auto iter = side_to_price_to_vols_[(int)s].begin();
		for (unsigned i = 0; i < position; ++i) ++iter;
		return &(*iter);
	}

	int get_order_count_on_side(side s) const
	{
		return side_to_price_to_vols_[(int)s].size();
	}

	double get_best_price(side s) const
	{
		return best_prices_[(int)s];
	}
	int get_volume(side s, double price) const
	{
		const auto orders_at_price = side_to_price_to_vols_[(int)s].equal_range(price);
		return std::accumulate(orders_at_price.first, orders_at_price.second,
				0,
				[](int accumulated, const side_map_type::value_type &elem)
				{
					return accumulated + elem.second;
				});
	}

	struct trade_stats
	{
		double last_trade_price = 0.0;
		uint64_t cumulative_trade_volume = 0;
	};
	const trade_stats &get_current_trade_stats() const
	{
		return trade_stats_;
	}

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
		std::pair<side, side_map_type::iterator> &side_and_iter = result->second;
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
		const std::pair<side, side_map_type::iterator> &side_and_iter = result->second;
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

	bool on_order_add(side s, int order_id, double price, int volume)
	{
		if (!check_validity(order_id, price, volume))
		{
			return false;
		}

		//if it exists already then something's wrong
		auto result = order_id_to_details_.insert(std::make_pair(order_id, std::make_pair(s, side_map_type::iterator())));
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


	void print_ob(std::ostream &os) const
	{
		//march through the two sides, printing each of their details in descending price order
		auto ask_iter = side_to_price_to_vols_[(int)side::ask].rbegin();
		auto bid_iter = side_to_price_to_vols_[(int)side::bid].begin();

		const auto end_ask = side_to_price_to_vols_[(int)side::ask].rend();
		const auto end_bid = side_to_price_to_vols_[(int)side::bid].end();
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

private:


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

	void update_midpoint()
	{
		//assume that midpoint is not valid if the book is crossed
		//we're making the assumption that if either side is zero that will already have been dealt with by
		// the update_best_prices method
		if (is_crossed())
		{
			midpoint_ = 0;
			return;
		}
		if (best_prices_[(int)side::ask] == 0 || best_prices_[(int)side::bid] == 0)
		{
			midpoint_ = 0;
			return;
		}
		const double diff = best_prices_[(int)side::ask] - best_prices_[(int)side::bid];
		midpoint_ = best_prices_[(int)side::bid] + (diff * 0.5);
	}

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


	//stats
	error_stats error_stats_;

	trade_stats trade_stats_;

	//in C++11 multimap guarantees stable ordering of elements with duplicate keys
	typedef std::function<bool(double, double)> comparator;
	using side_map_type = std::multimap<double, int, comparator>;
	std::vector<side_map_type> side_to_price_to_vols_;

	std::map<int, std::pair<side, side_map_type::iterator>> order_id_to_details_;

	std::vector<double> best_prices_;
	double midpoint_ = 0.0;
};

#endif


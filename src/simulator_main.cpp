//============================================================================
// Name        : feedhandler.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "orderbook.hpp"

#include <iostream>
#include <random>
#include <set>
#include <sstream>

typedef std::mt19937 MyRNG;  // the Mersenne Twister with a popular choice of parameters
MyRNG rng;

std::map<side, std::multimap<orderbook::ordered_price_to_volumes::value_type, int>> order_details_to_order_id_;
int next_order_id_ = 1;

int remove_order_mapping(side s, double price, int size)
{
	//get the order id
	auto iter = order_details_to_order_id_[s].find(std::make_pair(price, size));
	if (iter == order_details_to_order_id_[s].end())
	{
		std::cout << "Couldn't find " << s << ", " << price << ", " << size << " in order details map!" << std::endl;
		throw std::runtime_error("");
	}
	const auto order_id = iter->second;
	order_details_to_order_id_[s].erase(iter);
	return order_id;
}

void remove_order_mapping(side s, int order_id, double price, int size)
{
	auto iter_pair = order_details_to_order_id_[s].equal_range(std::make_pair(price, size));

	for (auto iter = iter_pair.first; iter != iter_pair.second; ++iter)
	{
		if (order_id == iter->second)
		{
			order_details_to_order_id_[s].erase(iter);
			return;
		}
	}

	std::cout << "Couldn't find order id " << order_id << " in our multimap!" << std::endl;
	throw std::logic_error("");
}

int modify_order_mapping(side s, double old_price, int old_size, double new_price, int new_size)
{
	//get the order id
	auto iter = order_details_to_order_id_[s].find(std::make_pair(old_price, old_size));
	const auto order_id = iter->second;
	order_details_to_order_id_[s].erase(iter);

	order_details_to_order_id_[s].emplace(std::make_pair(new_price, new_size), order_id);
	return order_id;
}


void modify_order_mapping(side s, int order_id, double old_price, int old_size, double new_price, int new_size)
{
	auto iter_pair = order_details_to_order_id_[s].equal_range(std::make_pair(old_price, old_size));

	for (auto iter = iter_pair.first; iter != iter_pair.second; ++iter)
	{
		if (order_id == iter->second)
		{
			order_details_to_order_id_[s].erase(iter);
			order_details_to_order_id_[s].emplace(std::make_pair(new_price, new_size), order_id);
			return;
		}
	}

	std::cout << "Couldn't find order id " << order_id << " in our multimap!" << std::endl;
	throw std::logic_error("");
}

enum class action
{
	add = 0,
	remove = 1,
	modify = 2
};

std::ostream& operator<<(std::ostream &os, action a)
{
	switch(a)
	{
	case action::add: return (os << "add");
	case action::remove: return (os << "remove");
	case action::modify: return (os << "modify");
	}
	return (os << "ERROR");
}

char encode_side(side s)
{
	return s == side::bid ? 'B' : 'S';
}

action generate_action()
{
	if (order_details_to_order_id_.empty())
	{
		return action::add;
	}

	static std::uniform_int_distribution<> distribution(0, 4);

	const auto result = distribution(rng);
	switch(result)
	{
	case 0:
		return action::remove;
	case 1:
		return action::modify;
	default:
		return action::add;
	}
}
side generate_side()
{
	static std::uniform_int_distribution<> distribution(0, 1);
	return (side)distribution(rng);
}

side generate_valid_side(const orderbook &ob)
{
	if (ob.get_midpoint() != 0)
	{
		return generate_side();
	}
	return ob.get_best_price(side::ask) == 0 ? side::bid : side::ask;
}

int generate_volume()
{
	static std::uniform_int_distribution<> distribution(1, 400);
	return distribution(rng);
}
double generate_double(double min, double max)
{
	std::uniform_real_distribution<> distribution(min, max);
	return distribution(rng);
}
int generate_int(int min, int max)
{
	std::uniform_int_distribution<> distribution(min, max);
	return distribution(rng);
}
bool generate_bool()
{
	static std::uniform_int_distribution<> distribution(0, 1);
	return distribution(rng) == 1;
}

double get_random_appropriate_price(const orderbook &ob, side s)
{
	const auto best_bid = ob.get_best_price(side::bid);
	const auto best_ask = ob.get_best_price(side::ask);

	//if nothing populated, return random double
	if (best_bid == 0 && best_ask == 0)
	{
		return (double)(std::uniform_int_distribution<>(100, 1000)(rng));
	}

	//if ask is empty then base it around best bid
	if (best_ask == 0)
	{
		return (double)std::uniform_int_distribution<>(best_bid * 0.8, best_bid * 1.2)(rng);
	}
	//if bid is empty base it around best ask
	if (best_bid == 0)
	{
		return (double)std::uniform_int_distribution<>(best_ask * 0.8, best_ask * 1.2)(rng);
	}

	//otherwise base it around the midpoint
	const auto midpoint = ob.is_crossed() ? ob.get_best_price(s) : ob.get_midpoint();

	if (s == side::bid)
	{
		const auto min_bound = std::max<double>(100, midpoint * 0.7);
		const auto max_bound = std::min<double>(1000, midpoint * 1.15);
		return (double)(int)std::uniform_real_distribution<>(min_bound, max_bound)(rng);
	}

	if (s == side::ask)
	{
		const auto min_bound = std::max<double>(100, midpoint * 0.85);
		const auto max_bound = std::min<double>(1000, midpoint * 1.3);
		return (double)(int)std::uniform_real_distribution<>(min_bound, max_bound)(rng);
	}
	throw std::logic_error("");
}

void initialize(int seed_val)
{
	rng.seed(seed_val);
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cout << "Must supply seed and number of events" << std::endl;
		return 1;
	}

	const int seed = atoi(argv[1]);
	const int num_events = atoi(argv[2]);

	initialize(seed);

	orderbook ob;

	for (auto i = 0; i < num_events; ++i)
	{
		const auto this_action = generate_action();

		switch (this_action)
		{
		//add - randomly generate an add on either side
		case action::add:
		{
			const auto order_id = next_order_id_++;
			const auto chosen_side = generate_side();
			const auto chosen_price = get_random_appropriate_price(ob, chosen_side);
			const auto chosen_volume = generate_volume();

			//insert the new order's details
			order_details_to_order_id_[chosen_side].emplace(std::make_pair(chosen_price, chosen_volume), order_id);
			ob.on_order_add(chosen_side, order_id, chosen_price, chosen_volume);

			std::cout << "A," << order_id << "," << encode_side(chosen_side) << "," << chosen_volume << "," << chosen_price << std::endl;

			break;
		}
		//modify - choose a random open order
		case action::modify:
		{
			const auto chosen_side = generate_valid_side(ob);
			const auto chosen_depth = generate_int(0, ob.get_order_count_on_side(chosen_side) - 1);
			const auto order_to_modify = *ob.get_order_in_position(chosen_side, chosen_depth);

			//do we change price?
			const auto new_price = generate_bool()
											? get_random_appropriate_price(ob, chosen_side)
											: order_to_modify.first;

			const auto new_size = generate_bool()
											? generate_volume()
											: order_to_modify.second;

			const auto order_id = modify_order_mapping(chosen_side, order_to_modify.first, order_to_modify.second, new_price, new_size);

			std::cout << "M," << order_id << "," << encode_side(chosen_side) << "," << new_size << "," << new_price << std::endl;

			ob.on_order_modify(chosen_side, order_id, new_price, new_size);

			break;
		}
		case action::remove:
		{
			const auto chosen_side = generate_valid_side(ob);
			const auto chosen_depth = generate_int(0, ob.get_order_count_on_side(chosen_side) - 1);
			const auto order_to_remove = *ob.get_order_in_position(chosen_side, chosen_depth);

			const auto order_id = remove_order_mapping(chosen_side, order_to_remove.first, order_to_remove.second);
			std::cout << "X," << order_id << "," << encode_side(chosen_side) << "," << order_to_remove.second << "," << order_to_remove.first << std::endl;

			ob.on_order_remove(chosen_side, order_id);

			break;
		}
		default:
			throw std::logic_error("");
		}

//		std::cout << std::endl << std::endl << "OB:" << std::endl;
//		ob.print_ob(std::cout);
//		std::cout << std::endl << std::endl;

		//if there's crossing, uncross everything until we're done
		if (ob.is_crossed())
		{
			//iterate over each side until we get rid of any overlap
			auto bid_touch_iter = ob.begin(side::bid);
			const auto bid_end = ob.end(side::bid);

			auto ask_touch_iter = ob.begin(side::ask);
			const auto ask_end = ob.end(side::ask);

			//they must currently overlap because we're crossed
			std::vector<std::string> trades;
			std::vector<std::string> order_actions;

			while (ob.is_crossed() && bid_touch_iter != bid_end && ask_touch_iter != ask_end)
			{
				auto remaining_vol_to_remove = bid_touch_iter->second;

				//we've got someone bidding for more than the ask price, remove the bid vol from ask
				auto price_to_sell = ask_touch_iter->first;

				//get the order id of touch
				const auto bid_order_id = order_details_to_order_id_[side::bid].find(*bid_touch_iter)->second;
				const auto ask_order_id = order_details_to_order_id_[side::ask].find(*ask_touch_iter)->second;

				//generate trade, modify or remove the bid, modify or remove the ask
				const auto this_trade_volume = std::min(remaining_vol_to_remove, ask_touch_iter->second);

				remaining_vol_to_remove -= this_trade_volume;

				{ //trade
					std::stringstream ss;
					ss << "T," << this_trade_volume << "," << price_to_sell;
					trades.push_back(ss.str());
				}

				{ //remove/modify bid
					if (remaining_vol_to_remove == 0)
					{
						std::stringstream ss;
						ss << "X," << bid_order_id << ",B," << bid_touch_iter->second << "," << bid_touch_iter->first;
						order_actions.push_back(ss.str());

						remove_order_mapping(side::bid, bid_order_id, bid_touch_iter->first, bid_touch_iter->second);

						//advance our bid iter so we don't get invalidated
						++bid_touch_iter;
						ob.on_order_remove(side::bid, bid_order_id);
					}
					else
					{
						std::stringstream ss;
						ss << "M," << bid_order_id << ",B," << remaining_vol_to_remove << "," << bid_touch_iter->first;
						order_actions.push_back(ss.str());

						modify_order_mapping(side::bid, bid_order_id, bid_touch_iter->first, bid_touch_iter->second, bid_touch_iter->first, remaining_vol_to_remove);

						ob.on_order_modify(side::bid, bid_order_id, bid_touch_iter->first, remaining_vol_to_remove);
					}
				}

				{ //remove/modify ask

					//non-zero left to remove so the ask order gets removed entirely
					if (remaining_vol_to_remove != 0)
					{
						std::stringstream ss;
						ss << "X," << ask_order_id << ",S," << ask_touch_iter->second << "," << ask_touch_iter->first;
						order_actions.push_back(ss.str());

						remove_order_mapping(side::ask, ask_order_id, ask_touch_iter->first, ask_touch_iter->second);

						//advance our ask iter so we don't get invalidated
						++ask_touch_iter;
						ob.on_order_remove(side::ask, ask_order_id);
					}
					else
					{
						const auto remaining_ask_volume = ask_touch_iter->second - this_trade_volume;

						std::stringstream ss;
						ss << "M," << ask_order_id << ",S," << remaining_ask_volume << "," << ask_touch_iter->first;
						order_actions.push_back(ss.str());

						modify_order_mapping(side::ask, ask_order_id, ask_touch_iter->first, ask_touch_iter->second, ask_touch_iter->first, remaining_ask_volume);

						ob.on_order_modify(side::ask, ask_order_id, ask_touch_iter->first, remaining_ask_volume);
					}

				}
			}

			//print out the trades then the actions
			for (const auto &trade : trades)
			{
				std::cout << trade << std::endl;
			}
			for (const auto &act : order_actions)
			{
				std::cout << act << std::endl;
			}

		}

		//ob.print_ob(std::cout);
	}

	return 0;
}

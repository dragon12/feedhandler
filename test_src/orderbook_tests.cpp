
#include "gtest/gtest.h"

#include "../src/orderbook.hpp"
#include "../src/feedhandler.hpp"

TEST(orderbook, sanity) {
	orderbook ob;

	EXPECT_EQ(0, ob.get_best_price(side::ask));
	EXPECT_EQ(0, ob.get_best_price(side::bid));
}

TEST(orderbook, single_add){
	orderbook ob;

	ob.on_order_add(side::ask, 1, 1.23, 321);
	EXPECT_EQ(nullptr, ob.get_order_in_position(side::bid, 0));

	const auto *ask_touch = ob.get_order_in_position(side::ask, 0);
	EXPECT_NE(nullptr, ask_touch);
	if (ask_touch)
	{
		EXPECT_DOUBLE_EQ(1.23, ask_touch->first);
		EXPECT_EQ(321, ask_touch->second);
	}
	EXPECT_FALSE(ob.is_crossed());
}

TEST(orderbook, multiple_adds){
	orderbook ob;

	ob.on_order_add(side::ask, 1, 1.23, 321);
	ob.on_order_add(side::ask, 2, 1.34, 432);
	ob.on_order_add(side::bid, 3, 1.21, 123);

	EXPECT_DOUBLE_EQ(1.22, ob.get_midpoint());

	const auto *bid_touch = ob.get_order_in_position(side::bid, 0);
	const auto *ask_touch = ob.get_order_in_position(side::ask, 0);
	EXPECT_NE(nullptr, bid_touch);
	EXPECT_NE(nullptr, ask_touch);
	if (ask_touch)
	{
		EXPECT_DOUBLE_EQ(1.23, ask_touch->first);
		EXPECT_EQ(321, ask_touch->second);
	}
	if (bid_touch)
	{
		EXPECT_DOUBLE_EQ(1.21, bid_touch->first);
		EXPECT_EQ(123, bid_touch->second);
	}
	EXPECT_FALSE(ob.is_crossed());
}

TEST(orderbook, crossing){
	orderbook ob;

	ob.on_order_add(side::ask, 1, 1.23, 321);
	ob.on_order_add(side::ask, 2, 1.34, 432);
	ob.on_order_add(side::bid, 3, 1.24, 123);


	EXPECT_DOUBLE_EQ(1.23, ob.get_best_price(side::ask));
	EXPECT_EQ(321, ob.get_volume(side::ask, 1.23));

	EXPECT_DOUBLE_EQ(1.24, ob.get_best_price(side::bid));
	EXPECT_EQ(123, ob.get_volume(side::bid, 1.24));

	EXPECT_TRUE(ob.is_crossed());
	EXPECT_DOUBLE_EQ(0, ob.get_midpoint());
}

TEST(orderbook, trade){
	orderbook ob;

	//set up the book to be crossed at 1.23
	EXPECT_TRUE(ob.on_order_add(side::bid, 1, 1.23, 1000));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.23, 1000));

	//trades at 1.23 are valid
	EXPECT_TRUE(ob.on_trade(1.23, 100));
	EXPECT_TRUE(ob.on_trade(1.23, 200));
	EXPECT_EQ(300, ob.get_current_trade_stats().cumulative_trade_volume);

	//now put an ask at 1.20 so that the trade appears valid
	EXPECT_TRUE(ob.on_order_add(side::ask, 3, 1.2, 800));

	EXPECT_TRUE(ob.on_trade(1.20, 500));
	const auto &stats = ob.get_current_trade_stats();
	EXPECT_EQ(500, stats.cumulative_trade_volume);
	EXPECT_DOUBLE_EQ(1.20, stats.last_trade_price);
}

//trades
TEST(orderbook, bad_trades)
{
	orderbook ob;

	//set up the book to be not crossed
	EXPECT_TRUE(ob.on_order_add(side::bid, 1, 1.2, 1000));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.3, 1000));

	//trades are invalid as the book is not crossed
	EXPECT_FALSE(ob.on_trade(1.2, 100));
	EXPECT_FALSE(ob.on_trade(1.3, 200));
	EXPECT_FALSE(ob.on_trade(1.25, 200));
	EXPECT_EQ(0, ob.get_current_trade_stats().cumulative_trade_volume);

	//now if we cross the book trades within the crossed slice are valid
	EXPECT_TRUE(ob.on_order_add(side::bid, 3, 1.28, 1000));
	EXPECT_TRUE(ob.on_order_add(side::ask, 4, 1.23, 1000));
	EXPECT_TRUE(ob.is_crossed());

	EXPECT_TRUE(ob.on_trade(1.23, 100));
	EXPECT_TRUE(ob.on_trade(1.25, 200));
	EXPECT_TRUE(ob.on_trade(1.28, 300));
	EXPECT_FALSE(ob.on_trade(1.22, 400));
	EXPECT_FALSE(ob.on_trade(1.29, 500));

	//trade stats should be 1.28, 300
	const auto &stats = ob.get_current_trade_stats();
	EXPECT_EQ(300, stats.cumulative_trade_volume);
	EXPECT_DOUBLE_EQ(1.28, stats.last_trade_price);
}

TEST(orderbook, volume_at_price)
{
	orderbook ob;

	ob.on_order_add(side::ask, 1, 1.2, 120);
	ob.on_order_add(side::ask, 2, 1.3, 130);
	ob.on_order_add(side::bid, 3, 1.1, 110);
	ob.on_order_add(side::ask, 4, 1.3, 70);

	EXPECT_EQ(120, ob.get_volume(side::ask, 1.2));
	EXPECT_EQ(0, ob.get_volume(side::bid, 1.2));
	EXPECT_EQ(0, ob.get_volume(side::ask, 1.4));
	EXPECT_EQ(110, ob.get_volume(side::bid, 1.1));
	EXPECT_EQ(200, ob.get_volume(side::ask, 1.3));
}

TEST(orderbook, add_remove)
{
	orderbook ob;
	ob.on_order_add(side::ask, 1, 1.2, 120);
	ob.on_order_add(side::ask, 2, 1.3, 130);
	ob.on_order_add(side::bid, 3, 1.1, 110);
	ob.on_order_add(side::ask, 4, 1.3, 70);
	EXPECT_EQ(120, ob.get_order_in_position(side::ask, 0)->second);
	ob.on_order_remove(side::ask, 1);
	EXPECT_EQ(130, ob.get_order_in_position(side::ask, 0)->second);
	EXPECT_FALSE(ob.is_crossed());

	ob.on_order_add(side::bid, 5, 1.3, 200);
	ob.on_order_remove(side::ask, 4);
	EXPECT_EQ(200, ob.get_volume(side::bid, 1.3));
	EXPECT_EQ(130, ob.get_volume(side::ask, 1.3));
	EXPECT_TRUE(ob.is_crossed());
}

//test that adds with duplicate order ids don't get added
TEST(orderbook, invalid_adds)
{
	orderbook ob;
	EXPECT_TRUE(ob.on_order_add(side::ask, 1, 1.2, 120));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.3, 130));
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_FALSE(ob.on_order_add(side::ask, 2, 1.4, 140));
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_EQ(0, ob.get_order_count_on_side(side::bid));
}

//test that removes with incorrect order id or incorrect side don't get removed
TEST(orderbook, invalid_removes)
{
	orderbook ob;
	EXPECT_TRUE(ob.on_order_add(side::ask, 1, 1.2, 120));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.3, 130));
	EXPECT_TRUE(ob.on_order_add(side::bid, 3, 1, 100));

	//try to remove something with a bad order id
	EXPECT_FALSE(ob.on_order_remove(side::ask, 3));

	//and now with a bad side for a good order id
	EXPECT_FALSE(ob.on_order_remove(side::bid, 2));

	//count should still be 2 asks, 1 bid
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_EQ(1, ob.get_order_count_on_side(side::bid));
}

TEST(orderbook, modifies)
{
	orderbook ob;
	EXPECT_TRUE(ob.on_order_add(side::ask, 1, 1.2, 120));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.3, 130));
	EXPECT_TRUE(ob.on_order_add(side::bid, 3, 1, 100));
	EXPECT_DOUBLE_EQ(1, ob.get_best_price(side::bid));
	EXPECT_DOUBLE_EQ(1.1, ob.get_midpoint());

	//modify the volume of order 2 and verify that it has been modified
	EXPECT_TRUE(ob.on_order_modify(side::ask, 2, 1.3, 150));
	EXPECT_EQ(150, ob.get_order_in_position(side::ask, 1)->second);
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_DOUBLE_EQ(1.2, ob.get_best_price(side::ask));
	EXPECT_DOUBLE_EQ(1.1, ob.get_midpoint());

	//modify the price of order 1 and verify that it is now behind the existing 1.3 price
	EXPECT_TRUE(ob.on_order_modify(side::ask, 1, 1.3, 120));
	EXPECT_EQ(120, ob.get_order_in_position(side::ask, 1)->second);
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_DOUBLE_EQ(1.3, ob.get_best_price(side::ask));
	EXPECT_DOUBLE_EQ(1.15, ob.get_midpoint());


	//modify the price and size of order 2 so that it is again behind order 1
	EXPECT_TRUE(ob.on_order_modify(side::ask, 2, 1.4, 200));
	EXPECT_EQ(120, ob.get_order_in_position(side::ask, 0)->second);
	EXPECT_EQ(200, ob.get_order_in_position(side::ask, 1)->second);
	EXPECT_EQ(2, ob.get_order_count_on_side(side::ask));
	EXPECT_DOUBLE_EQ(1.3, ob.get_best_price(side::ask));
	EXPECT_DOUBLE_EQ(1.15, ob.get_midpoint());

	//modify the size of order 3 to be zero, should result in it being removed
	EXPECT_TRUE(ob.on_order_modify(side::bid, 3, 1, 0));
	EXPECT_EQ(0, ob.get_order_count_on_side(side::bid));
	EXPECT_DOUBLE_EQ(0, ob.get_best_price(side::bid));
	EXPECT_DOUBLE_EQ(0, ob.get_midpoint());
}

TEST(orderbook, invalid_modifies)
{
	orderbook ob;
	EXPECT_TRUE(ob.on_order_add(side::ask, 1, 10, 100));

	//modify for non-existent order id should fail
	EXPECT_FALSE(ob.on_order_modify(side::ask, 2, 10, 100));

	//modify for good order id but wrong side should fail
	EXPECT_FALSE(ob.on_order_modify(side::bid, 1, 10, 100));

	EXPECT_DOUBLE_EQ(10, ob.get_best_price(side::ask));

	//this modify should succeed
	EXPECT_TRUE(ob.on_order_modify(side::ask, 1, 10, 200));
	EXPECT_DOUBLE_EQ(10, ob.get_best_price(side::ask));
	EXPECT_EQ(200, ob.get_order_in_position(side::ask, 0)->second);

	//now remove the order, modifying it again should fail
	EXPECT_TRUE(ob.on_order_remove(side::ask, 1));
	EXPECT_FALSE(ob.on_order_modify(side::ask, 1, 10, 200));
}

TEST(orderbook, out_of_bounds_values)
{
	orderbook ob;
	EXPECT_TRUE(ob.on_order_add(side::ask, 1, 1.2, 120));
	EXPECT_TRUE(ob.on_order_add(side::ask, 2, 1.3, 130));
	EXPECT_TRUE(ob.on_order_add(side::bid, 3, 1, 100));

	EXPECT_FALSE(ob.on_order_add(side::ask, -1, 10, 100));
	EXPECT_FALSE(ob.on_order_add(side::ask, 4, -1, 100));
	EXPECT_FALSE(ob.on_order_add(side::ask, 4, 1.4, -1));

	EXPECT_FALSE(ob.on_order_remove(side::ask, -1));

	EXPECT_FALSE(ob.on_order_modify(side::ask, -1, 10, 100));
	EXPECT_FALSE(ob.on_order_modify(side::ask, 2, -1, 100));
	EXPECT_FALSE(ob.on_order_modify(side::ask, 2, 10, -1));

	EXPECT_EQ(7, ob.get_error_stats().invalid_inputs);
}


#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <ctime>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <gnuplot.h>

class Order {
public:
    std::string order_type;
    std::time_t order_time;
    int price;
    int quantity;
    int time_to_kill;
    int order_id;

    Order(std::string type, std::time_t time, int p, int q, int ttk, int id)
        : order_type(type), order_time(time), price(p), quantity(q), time_to_kill(ttk), order_id(id) {}

    void print_order() {
        std::cout << order_type << " " << std::ctime(&order_time) << " " << price << " " << quantity << " " << time_to_kill << " " << order_id << std::endl;
    }
};

class Ticker {
private:
    std::mutex mtx;

public:
    int price;
    std::map<int, std::deque<Order>> buys;
    std::map<int, std::deque<Order>> sells;
    std::vector<std::tuple<std::vector<std::time_t>, std::vector<int>, std::vector<std::time_t>, std::vector<int>>> plots;

    void place_order(Order order) {
        std::lock_guard<std::mutex> lock(mtx);
        if (order.order_type == "BUY") {
            buys[order.price].push_back(order);
        } else if (order.order_type == "SELL") {
            sells[order.price].push_back(order);
        }
    }

    void print_orders() {
        std::cout << "Buys:\n";
        for (auto& [price, orders] : buys) {
            for (auto& order : orders) {
                order.print_order();
            }
        }
        std::cout << "Sells:\n";
        for (auto& [price, orders] : sells) {
            for (auto& order : orders) {
                order.print_order();
            }
        }
    }

    std::map<int, std::set<std::tuple<int, int, int>>> match(std::time_t curr_time) {
        std::lock_guard<std::mutex> lock(mtx);
        std::map<int, std::set<std::tuple<int, int, int>>> matches;

        for (auto& [price, buy_orders] : buys) {
            if (sells.find(price) != sells.end()) {
                auto& sell_orders = sells[price];
                std::set<std::tuple<int, int, int>> price_matched;

                while (!buy_orders.empty() && !sell_orders.empty()) {
                    auto& buy = buy_orders.front();
                    auto& sell = sell_orders.front();

                    if (std::difftime(curr_time, buy.order_time) > buy.time_to_kill || buy.quantity == 0) {
                        buy_orders.pop_front();
                    } else if (std::difftime(curr_time, sell.order_time) > sell.time_to_kill || sell.quantity == 0) {
                        sell_orders.pop_front();
                    } else {
                        int remove_quantity = std::min(buy.quantity, sell.quantity);
                        buy.quantity -= remove_quantity;
                        sell.quantity -= remove_quantity;
                        price_matched.insert(std::make_tuple(remove_quantity, buy.order_id, sell.order_id));

                        if (buy.quantity == 0) {
                            buy_orders.pop_front();
                        }
                        if (sell.quantity == 0) {
                            sell_orders.pop_front();
                        }
                    }
                }
                if (!price_matched.empty()) {
                    matches[price] = price_matched;
                }
            }
        }
        return matches;
    }

    void plot() {
        std::vector<std::time_t> x_buy;
        std::vector<int> y_buy;
        for (auto& [price, orders] : buys) {
            for (auto& order : orders) {
                x_buy.push_back(order.order_time);
                y_buy.push_back(order.price);
            }
        }

        std::vector<std::time_t> x_sell;
        std::vector<int> y_sell;
        for (auto& [price, orders] : sells) {
            for (auto& order : orders) {
                x_sell.push_back(order.order_time);
                y_sell.push_back(order.price);
            }
        }

        plots.push_back(std::make_tuple(x_buy, y_buy, x_sell, y_sell));
    }
};

int myround(int x, int base = 5) {
    return base * ((x + base / 2) / base);
}

Order generate_order() {
    std::vector<std::string> order_types = { "BUY", "SELL" };
    std::time_t curr_time = std::time(nullptr);
    return Order(
        order_types[rand() % 2],
        curr_time,
        myround(rand() % 101),
        rand() % 101,
        rand() % 8 + 3,
        1
    );
}

void generate_orders(Ticker& ticker) {
    int i = 0;
    while (i <= 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        Order order = generate_order();
        ticker.place_order(order);
        i++;
    }
}

void operate(Ticker& ticker) {
    int i = 0;
    while (i <= 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::time_t t = std::time(nullptr);
        if (i % 5 == 0) {
            ticker.plot();
        } else {
            ticker.match(t);
        }
        i++;
    }
}

int main() {
    Ticker Meta;
    std::thread t1(generate_orders, std::ref(Meta));
    std::thread t2(operate, std::ref(Meta));

    t1.join();
    t2.join();

    int num_plots = Meta.plots.size();
    gnuplot_ctrl* h = gnuplot_init();
    gnuplot_setstyle(h, "points");

    for (int i = 0; i < num_plots; ++i) {
        auto& [x_buy, y_buy, x_sell, y_sell] = Meta.plots[i];
        std::vector<double> x_buy_d(x_buy.size());
        std::vector<double> x_sell_d(x_sell.size());
        std::transform(x_buy.begin(), x_buy.end(), x_buy_d.begin(), [](std::time_t t) { return static_cast<double>(t); });
        std::transform(x_sell.begin(), x_sell.end(), x_sell_d.begin(), [](std::time_t t) { return static_cast<double>(t); });

        gnuplot_resetplot(h);
        gnuplot_plot_xy(h, x_buy_d.data(), y_buy.data(), x_buy.size(), "Buy");
        gnuplot_plot_xy(h, x_sell_d.data(), y_sell.data(), x_sell.size(), "Sell");
    }

    gnuplot_close(h);

    return 0;
}

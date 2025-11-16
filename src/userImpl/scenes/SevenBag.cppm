module;
#include <algorithm>
#include <deque>
#include <random>
export module SevenBag;
import Tetrimino;
/**
 * @brief テトリミノキュー
 * @param queue テトリミノ種別のキュー
 * @param rng 乱数生成器
 */
export struct PieceQueue {
    std::deque<PieceType> queue;
    std::mt19937 rng{std::random_device{}()};
};

export inline void refill_bag(PieceQueue& pq) {
    std::array<PieceType, 7> bag{PieceType::I, PieceType::O, PieceType::T, PieceType::S,
                                 PieceType::Z, PieceType::J, PieceType::L};
    std::shuffle(bag.begin(), bag.end(), pq.rng);
    for (auto t : bag) pq.queue.push_back(t);
}

export inline PieceType take_next(PieceQueue& pq) {
    if (pq.queue.empty()) refill_bag(pq);
    auto t = pq.queue.front();
    pq.queue.pop_front();
    return t;
}

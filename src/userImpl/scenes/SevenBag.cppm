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
    // キューが空ならとりあえず1Bag補充（初期化用）
    if (pq.queue.empty()) {
        refill_bag(pq);
    }

    // 先頭を1つ取り出す
    auto t = pq.queue.front();
    pq.queue.pop_front();

    // ★ ここで「2Bag 目」を常に用意しておく
    //    7 個未満になったら次の Bag を足す
    constexpr std::size_t bag_size = 7;
    if (pq.queue.size() < bag_size) {
        refill_bag(pq);
    }

    return t;
}


/**
 * @brief 現在のキューを読み取り専用で返す
 * @param pq テトリミノキュー
 * @return const 参照としての deque
 */
export inline const std::deque<PieceType>& view_queue(const PieceQueue& pq) { return pq.queue; }

#include <cstring>
#include <cstdint>
#include <cassert>

#include <iostream>
#include <random>
#include <chrono>
#include <set>
#include <map>

#include "../utils/serialization.h"
#include "../utils/log.h"
#include "../fast_base64/chromiumbase64.h"

char bases64[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
                  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
                  'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
                  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                  'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
                  'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
                  '8', '9', '+', '/'};
char bases36[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                  'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
                  '4', '5', '6', '7', '8', '9'};

#define BASE64_SIZE 64
#define BASE36_SIZE 36
#define MSG_NUM 100000000
#define ARR_SIZE 209715200
#define MSG_LEN 50
#define SMALL_MSG_LEN 30

using namespace std;
using namespace std::chrono;

void test_efficiency_base64() {
    random_device rd;
    std::uniform_int_distribution<int> distribution(0, BASE64_SIZE - 1);
    auto *msg_arr = new char[ARR_SIZE];    //200MB

#pragma omp parallel
    {
        random_device rd_local;
        mt19937 gen_local(rd_local());
        std::uniform_int_distribution<int> distribution_local(0, BASE64_SIZE - 1);
#pragma omp for
        for (int i = 0; i < ARR_SIZE; i++) {
            int idx = distribution_local(gen_local);
            assert(idx >= 0 && idx < BASE64_SIZE);
            msg_arr[i] = bases64[idx];
            assert(msg_arr[i] > 0);
        }
    }

    std::uniform_int_distribution<int> distribution_idx(0, ARR_SIZE - MSG_LEN);
    char intermediate[MSG_LEN + FIXED_PART_LEN];
    log_info("finish init");

    auto start = high_resolution_clock::now();
    for (int i = 0; i < MSG_NUM; i++) {
        int idx = i % (ARR_SIZE - MSG_LEN);
        // 1st: message, serialize
        memcpy(intermediate, msg_arr + idx, MSG_LEN);
        memcpy(intermediate + MSG_LEN, &idx, 4);
        memcpy(intermediate + MSG_LEN + 4, &idx, 4);

        auto *serialized = new char[MSG_LEN + FIXED_PART_LEN + 64];
        int middle_len = serialize_base64_decoding_general(reinterpret_cast<uint8_t *>(intermediate),
                                                           MSG_LEN + 8,
                                                           reinterpret_cast<uint8_t *>(serialized));
        static thread_local bool is_first = true;
        if (is_first) {
            is_first = false;
            log_info("%d", middle_len);
        }
#ifdef ENABLE_DESERIALIZE
        // 2nd: deserialize
        int length;
        auto deserialized = deserialize_base64_encoding_general(reinterpret_cast<const uint8_t *>(serialized),
                                                                middle_len, length);
        assert(length == MSG_LEN + 8);
        assert(memcmp(msg_arr + idx, deserialized, MSG_LEN) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN, 4) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN + 4, 4) == 0);
        delete[]deserialized;
#endif

        delete[]serialized;
    }

    auto end = high_resolution_clock::now();
    log_info("elapsed time: %.6fs", duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)));
    log_info("TPS: %f MB/s", (MSG_LEN + FIXED_PART_LEN - BASE64_INFO_LEN) * static_cast<double>(MSG_NUM) /
                             (duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)))
                             / (1024 * 1024));
    delete[]msg_arr;
}

void test_efficiency_base64_skip_index() {
    random_device rd;
    std::uniform_int_distribution<int> distribution(0, BASE64_SIZE - 1);
    auto *msg_arr = new char[ARR_SIZE];    //200MB

#pragma omp parallel
    {
        random_device rd_local;
        mt19937 gen_local(rd_local());
        std::uniform_int_distribution<int> distribution_local(0, BASE64_SIZE - 1);
#pragma omp for
        for (int i = 0; i < ARR_SIZE; i++) {
            int idx = distribution_local(gen_local);
            assert(idx >= 0 && idx < BASE64_SIZE);
            msg_arr[i] = bases64[idx];
            assert(msg_arr[i] > 0);
        }
    }

    std::uniform_int_distribution<int> distribution_idx(0, ARR_SIZE - MSG_LEN);
    char intermediate[MSG_LEN + FIXED_PART_LEN];
    log_info("finish init");

    auto start = high_resolution_clock::now();
    for (int i = 0; i < MSG_NUM; i++) {
        int idx = i % (ARR_SIZE - MSG_LEN);
        // 1st: message, serialize
        memcpy(intermediate, msg_arr + idx, MSG_LEN);
        memcpy(intermediate + MSG_LEN, &idx, 4);
        memcpy(intermediate + MSG_LEN + 4, &idx, 4);

        auto *serialized = new char[MSG_LEN + FIXED_PART_LEN + 64];
        int middle_len = serialize_base64_decoding_skip_index(reinterpret_cast<uint8_t *>(intermediate),
                                                              MSG_LEN + 8,
                                                              reinterpret_cast<uint8_t *>(serialized));

#ifdef ENABLE_DESERIALIZE
        // 2nd: deserialize
        int length;
        auto deserialized = deserialize_base64_encoding_add_index(reinterpret_cast<const uint8_t *>(serialized),
                                                                  middle_len, length, idx);
        assert(length == MSG_LEN + 8);
        assert(memcmp(msg_arr + idx, deserialized, MSG_LEN) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN, 4) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN + 4, 4) == 0);
        delete[]deserialized;
#endif
        delete[]serialized;
    }

    auto end = high_resolution_clock::now();
    log_info("elapsed time: %.6fs", duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)));
    log_info("TPS: %f MB/s", (MSG_LEN + FIXED_PART_LEN - BASE64_INFO_LEN) * static_cast<double>(MSG_NUM) /
                             (duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)))
                             / (1024 * 1024));
    delete[]msg_arr;
}

void test_efficiency_base36_skip_index() {
    random_device rd;
    std::uniform_int_distribution<int> distribution(0, BASE36_SIZE - 1);
    auto *msg_arr = new char[ARR_SIZE];    //200MB

#pragma omp parallel
    {
        random_device rd_local;
        mt19937 gen_local(rd_local());
        std::uniform_int_distribution<int> distribution_local(0, BASE36_SIZE - 1);
#pragma omp for
        for (int i = 0; i < ARR_SIZE; i++) {
            int idx = distribution_local(gen_local);
            assert(idx >= 0 && idx < BASE36_SIZE);
            msg_arr[i] = bases36[idx];
            assert(msg_arr[i] > 0);
        }
    }

    std::uniform_int_distribution<int> distribution_idx(0, ARR_SIZE - MSG_LEN);
    char intermediate[MSG_LEN + FIXED_PART_LEN];
    log_info("finish init");

    auto start = high_resolution_clock::now();
    for (int i = 0; i < MSG_NUM; i++) {
        int idx = i % (ARR_SIZE - MSG_LEN);
        // 1st: message, serialize
        memcpy(intermediate, msg_arr + idx, MSG_LEN);
        memcpy(intermediate + MSG_LEN, &idx, 4);
        memcpy(intermediate + MSG_LEN + 4, &idx, 4);

        auto *serialized = new char[MSG_LEN + FIXED_PART_LEN + 64];
        int middle_len = serialize_base36_decoding_skip_index(reinterpret_cast<uint8_t *>(intermediate),
                                                              MSG_LEN + 8,
                                                              reinterpret_cast<uint8_t *>(serialized));
#ifdef ENABLE_DESERIALIZE
        // 2nd: deserialize
        int length;
        auto deserialized = deserialize_base36_encoding_add_index(reinterpret_cast<const uint8_t *>(serialized),
                                                                  middle_len, length, idx);
        assert(length == MSG_LEN + 8);
        assert(memcmp(msg_arr + idx, deserialized, MSG_LEN) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN, 4) == 0);
        assert(memcmp(&idx, deserialized + MSG_LEN + 4, 4) == 0);
        delete[]deserialized;
#endif
        delete[]serialized;
    }

    auto end = high_resolution_clock::now();
    log_info("elapsed time: %.6fs", duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)));
    log_info("TPS: %f MB/s", (MSG_LEN + FIXED_PART_LEN - BASE64_INFO_LEN) * static_cast<double>(MSG_NUM) /
                             (duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)))
                             / (1024 * 1024));
    delete[]msg_arr;
}

void test_efficiency_base36_skip_index_support_small_msg() {
    random_device rd;
    std::uniform_int_distribution<int> distribution(0, BASE36_SIZE - 1);
    auto *msg_arr = new char[ARR_SIZE];    //200MB

#pragma omp parallel
    {
        random_device rd_local;
        mt19937 gen_local(rd_local());
        std::uniform_int_distribution<int> distribution_local(0, BASE36_SIZE - 1);
#pragma omp for
        for (int i = 0; i < ARR_SIZE; i++) {
            int idx = distribution_local(gen_local);
            assert(idx >= 0 && idx < BASE36_SIZE);
            msg_arr[i] = bases36[idx];
            assert(msg_arr[i] > 0);
        }
    }

    std::uniform_int_distribution<int> distribution_idx(0, ARR_SIZE - SMALL_MSG_LEN);
    char intermediate[SMALL_MSG_LEN + FIXED_PART_LEN];
    log_info("finish init");

    auto start = high_resolution_clock::now();
    for (int i = 0; i < MSG_NUM; i++) {
        int idx = i % (ARR_SIZE - SMALL_MSG_LEN);
        // 1st: message, serialize
        memcpy(intermediate, msg_arr + idx, SMALL_MSG_LEN);
        memcpy(intermediate + SMALL_MSG_LEN, &idx, 4);
        memcpy(intermediate + SMALL_MSG_LEN + 4, &idx, 4);

        auto *serialized = new char[SMALL_MSG_LEN + FIXED_PART_LEN + 64];
        int middle_len = serialize_base36_decoding_with_padding_skip_index(reinterpret_cast<uint8_t *>(intermediate),
                                                                           SMALL_MSG_LEN + 8,
                                                                           reinterpret_cast<uint8_t *>(serialized));
#ifdef ENABLE_DESERIALIZE
        // 2nd: deserialize
        int length;
        auto deserialized = deserialize_base36_encoding_add_index(reinterpret_cast<const uint8_t *>(serialized),
                                                                  middle_len, length, idx);
        assert(length == SMALL_MSG_LEN + 8);
        assert(memcmp(msg_arr + idx, deserialized, SMALL_MSG_LEN) == 0);
        assert(memcmp(&idx, deserialized + SMALL_MSG_LEN, 4) == 0);
        assert(memcmp(&idx, deserialized + SMALL_MSG_LEN + 4, 4) == 0);
        delete[]deserialized;
#endif
        delete[]serialized;
    }

    auto end = high_resolution_clock::now();
    log_info("elapsed time: %.6fs", duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)));
    log_info("TPS: %f MB/s", (SMALL_MSG_LEN + FIXED_PART_LEN - BASE64_INFO_LEN) * static_cast<double>(MSG_NUM) /
                             (duration_cast<microseconds>(end - start).count() / static_cast<float>(pow(10, 6)))
                             / (1024 * 1024));
    delete[]msg_arr;
}

int main() {
    test_efficiency_base64();
    test_efficiency_base64_skip_index();
    test_efficiency_base36_skip_index();
    test_efficiency_base36_skip_index_support_small_msg();
}

#include <algorithm>
#include <array>
#include <cctype>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "mahjong/mahjong.hpp"

namespace py = pybind11;

namespace
{
int parse_wind(const std::string &wind)
{
    if (wind == "E" || wind == "e" || wind == "east" || wind == "East" ||
        wind == "1z") {
        return mahjong::Tile::East;
    }
    if (wind == "S" || wind == "s" || wind == "south" || wind == "South" ||
        wind == "2z") {
        return mahjong::Tile::South;
    }
    if (wind == "W" || wind == "w" || wind == "west" || wind == "West" ||
        wind == "3z") {
        return mahjong::Tile::West;
    }
    if (wind == "N" || wind == "n" || wind == "north" || wind == "North" ||
        wind == "4z") {
        return mahjong::Tile::North;
    }

    throw std::invalid_argument("wind must be one of E, S, W, N, 1z, 2z, 3z, 4z");
}

mahjong::Hand from_mpsz_loose(const std::string &tiles)
{
    mahjong::Hand hand{0};

    char type = '\0';
    for (auto it = tiles.rbegin(); it != tiles.rend(); ++it) {
        const unsigned char ch = static_cast<unsigned char>(*it);
        if (std::isspace(ch)) {
            continue;
        }

        if (*it == 'm' || *it == 'p' || *it == 's' || *it == 'z') {
            type = *it;
            continue;
        }

        if (!std::isdigit(ch)) {
            throw std::invalid_argument("invalid mpsz tile string");
        }

        if (type == '\0') {
            throw std::invalid_argument("mpsz digits must be followed by a suit");
        }

        const int number = *it - '0';
        if (type == 'm') {
            if (number == 0) {
                ++hand[mahjong::Tile::RedManzu5];
                ++hand[mahjong::Tile::Manzu5];
            }
            else if (1 <= number && number <= 9) {
                ++hand[mahjong::Tile::Manzu1 + number - 1];
            }
            else {
                throw std::invalid_argument("invalid manzu tile in mpsz string");
            }
        }
        else if (type == 'p') {
            if (number == 0) {
                ++hand[mahjong::Tile::RedPinzu5];
                ++hand[mahjong::Tile::Pinzu5];
            }
            else if (1 <= number && number <= 9) {
                ++hand[mahjong::Tile::Pinzu1 + number - 1];
            }
            else {
                throw std::invalid_argument("invalid pinzu tile in mpsz string");
            }
        }
        else if (type == 's') {
            if (number == 0) {
                ++hand[mahjong::Tile::RedSouzu5];
                ++hand[mahjong::Tile::Souzu5];
            }
            else if (1 <= number && number <= 9) {
                ++hand[mahjong::Tile::Souzu1 + number - 1];
            }
            else {
                throw std::invalid_argument("invalid souzu tile in mpsz string");
            }
        }
        else if (type == 'z') {
            if (1 <= number && number <= 7) {
                ++hand[mahjong::Tile::East + number - 1];
            }
            else {
                throw std::invalid_argument("invalid honor tile in mpsz string");
            }
        }
    }

    return hand;
}

std::vector<int> hand_to_tiles(const mahjong::Hand &hand)
{
    std::vector<int> tiles;
    for (int tile = 34; tile < 37; ++tile) {
        for (int i = 0; i < hand[tile]; ++i) {
            tiles.push_back(tile);
        }
    }
    for (int tile = 0; tile < 34; ++tile) {
        int count = hand[tile];
        if (tile == mahjong::Tile::Manzu5) {
            count -= hand[mahjong::Tile::RedManzu5];
        }
        else if (tile == mahjong::Tile::Pinzu5) {
            count -= hand[mahjong::Tile::RedPinzu5];
        }
        else if (tile == mahjong::Tile::Souzu5) {
            count -= hand[mahjong::Tile::RedSouzu5];
        }

        for (int i = 0; i < count; ++i) {
            tiles.push_back(tile);
        }
    }

    return tiles;
}

int parse_single_tile(const std::string &tile_mpsz)
{
    const mahjong::Hand hand = mahjong::from_mpsz(tile_mpsz);
    const std::vector<int> tiles = hand_to_tiles(hand);
    if (tiles.size() != 1) {
        throw std::invalid_argument("each dora indicator must contain exactly one tile");
    }

    return tiles.front();
}

int parse_meld_type(const std::string &type)
{
    if (type == "pon" || type == "pong") {
        return mahjong::MeldType::Pong;
    }
    if (type == "chi" || type == "chow") {
        return mahjong::MeldType::Chow;
    }
    if (type == "ankan" || type == "closed_kan" || type == "closed_kong") {
        return mahjong::MeldType::ClosedKong;
    }
    if (type == "kan" || type == "minkan" || type == "open_kan" ||
        type == "open_kong") {
        return mahjong::MeldType::OpenKong;
    }
    if (type == "kakan" || type == "added_kan" || type == "added_kong") {
        return mahjong::MeldType::AddedKong;
    }

    throw std::invalid_argument(
        "meld type must be one of chi, pon, kan, ankan, kakan");
}

std::vector<mahjong::Meld> parse_melds(const py::object &melds)
{
    std::vector<mahjong::Meld> parsed;
    if (melds.is_none()) {
        return parsed;
    }

    for (const py::handle item : py::reinterpret_borrow<py::iterable>(melds)) {
        std::string type;
        std::string tiles_mpsz;

        if (py::isinstance<py::dict>(item)) {
            const py::dict meld = py::reinterpret_borrow<py::dict>(item);
            type = py::str(meld["type"]).cast<std::string>();
            tiles_mpsz = py::str(meld["tiles"]).cast<std::string>();
        }
        else {
            const py::sequence meld = py::reinterpret_borrow<py::sequence>(item);
            if (py::len(meld) < 2) {
                throw std::invalid_argument(
                    "meld entries must be dicts or (type, tiles_mpsz) pairs");
            }
            type = py::str(meld[0]).cast<std::string>();
            tiles_mpsz = py::str(meld[1]).cast<std::string>();
        }

        std::vector<int> tiles = hand_to_tiles(from_mpsz_loose(tiles_mpsz));
        parsed.emplace_back(parse_meld_type(type), tiles);
    }

    return parsed;
}

void remove_visible_tile(mahjong::Count &wall, const int tile)
{
    const int no_red_tile = mahjong::to_no_reddora(tile);
    --wall[no_red_tile];
    if (mahjong::is_reddora(tile)) {
        --wall[tile];
    }

    if (wall[no_red_tile] < 0 ||
        (mahjong::is_reddora(tile) && wall[tile] < 0)) {
        throw std::invalid_argument(
            "visible_tiles_mpsz contains more visible copies than remain in wall");
    }
}

void remove_visible_tiles(mahjong::Count &wall, const std::string &visible_tiles_mpsz)
{
    if (visible_tiles_mpsz.empty()) {
        return;
    }

    const std::vector<int> tiles = hand_to_tiles(from_mpsz_loose(visible_tiles_mpsz));
    for (const int tile : tiles) {
        remove_visible_tile(wall, tile);
    }
}

float safe_probability(const std::vector<double> &values, const int turn)
{
    if (turn < 0 || turn >= static_cast<int>(values.size())) {
        return 0.0f;
    }

    return static_cast<float>(std::clamp(values[turn], 0.0, 1.0));
}

float safe_score_norm(const std::vector<double> &values, const int turn)
{
    if (turn < 0 || turn >= static_cast<int>(values.size())) {
        return 0.0f;
    }

    return static_cast<float>(std::min(values[turn], 32000.0) / 32000.0);
}

int safe_max_score(const std::vector<int> &values, const int turn)
{
    if (turn < 0 || turn >= static_cast<int>(values.size())) {
        return 0;
    }

    return values[turn];
}

py::array_t<float> calc_expected_features(
    const std::string &hand_mpsz, const std::string &bakaze,
    const std::string &jikaze, const std::vector<std::string> &dora_indicators,
    const std::string &visible_tiles_mpsz, const py::object &melds, const int t_min,
    const int t_max, const int extra, const bool enable_reddora,
    const bool enable_uradora, const bool enable_shanten_down,
    const bool enable_tegawari, const bool enable_riichi)
{
    mahjong::Player player;
    player.hand = mahjong::from_mpsz(hand_mpsz);
    player.melds = parse_melds(melds);
    player.wind = parse_wind(jikaze);

    mahjong::Round round;
    round.rules = mahjong::RuleFlag::OpenTanyao |
                  (enable_reddora ? mahjong::RuleFlag::RedDora
                                  : mahjong::RuleFlag::Null);
    round.wind = parse_wind(bakaze);
    for (const std::string &indicator : dora_indicators) {
        round.dora_indicators.push_back(parse_single_tile(indicator));
    }

    mahjong::ExpectedScoreCalculator::Config config;
    config.t_min = std::clamp(t_min, 1, 6);
    config.t_max = std::max(t_max, 12);
    config.extra = extra;
    config.shanten_type = mahjong::ShantenFlag::All;
    config.enable_reddora = enable_reddora;
    config.enable_uradora = enable_uradora;
    config.enable_shanten_down = enable_shanten_down;
    config.enable_tegawari = enable_tegawari;
    config.enable_riichi = enable_riichi;
    config.calc_stats = true;

    mahjong::Count wall =
        mahjong::ExpectedScoreCalculator::create_wall(round, player, enable_reddora);
    remove_visible_tiles(wall, visible_tiles_mpsz);

    const auto [stats, searched] =
        mahjong::ExpectedScoreCalculator::calc(config, round, player, wall);
    (void)searched;

    py::array_t<float> result(
        {static_cast<py::ssize_t>(12), static_cast<py::ssize_t>(34)});
    auto features = result.mutable_unchecked<2>();
    for (py::ssize_t c = 0; c < features.shape(0); ++c) {
        for (py::ssize_t t = 0; t < features.shape(1); ++t) {
            features(c, t) = 0.0f;
        }
    }

    std::array<bool, 34> seen{};
    for (const auto &stat : stats) {
        if (stat.tile < 0) {
            continue;
        }

        const int tile = mahjong::to_no_reddora(stat.tile);
        if (tile < 0 || tile >= 34) {
            continue;
        }

        const int necessary_count =
            std::accumulate(stat.necessary_tiles.begin(), stat.necessary_tiles.end(),
                            0, [](int sum, const auto &tile_count) {
                                return sum + std::get<1>(tile_count);
                            });
        const int max_score_12 = safe_max_score(stat.max_score, 12);

        const float shanten_norm =
            static_cast<float>(std::clamp(stat.shanten, 0, 6)) / 6.0f;
        const float necessary_norm =
            static_cast<float>(std::min(necessary_count, 34)) / 34.0f;
        const float max_score_12_norm =
            static_cast<float>(std::min(max_score_12, 32000)) / 32000.0f;

        if (!seen[tile]) {
            features(0, tile) = shanten_norm;
            features(1, tile) = necessary_norm;
            seen[tile] = true;
        }
        else {
            features(0, tile) = std::min(features(0, tile), shanten_norm);
            features(1, tile) = std::max(features(1, tile), necessary_norm);
        }

        features(2, tile) =
            std::max(features(2, tile), safe_probability(stat.tenpai_prob, 6));
        features(3, tile) =
            std::max(features(3, tile), safe_probability(stat.tenpai_prob, 12));
        features(4, tile) =
            std::max(features(4, tile), safe_probability(stat.win_prob, 6));
        features(5, tile) =
            std::max(features(5, tile), safe_probability(stat.win_prob, 12));
        features(6, tile) =
            std::max(features(6, tile), safe_score_norm(stat.exp_score, 6));
        features(7, tile) =
            std::max(features(7, tile), safe_score_norm(stat.exp_score, 12));
        features(8, tile) = std::max(features(8, tile), max_score_12_norm);
        features(9, tile) =
            std::max(features(9, tile), max_score_12 >= 3900 ? 1.0f : 0.0f);
        features(10, tile) =
            std::max(features(10, tile), max_score_12 >= 8000 ? 1.0f : 0.0f);
        features(11, tile) =
            std::max(features(11, tile), max_score_12 >= 12000 ? 1.0f : 0.0f);
    }

    return result;
}
} // namespace

PYBIND11_MODULE(nekoriichi, m)
{
    m.doc() = "Python bindings for mahjong-cpp expected-score features";
    m.def("calc_expected_features", &calc_expected_features,
          py::arg("hand_mpsz"), py::arg("bakaze") = "E", py::arg("jikaze") = "E",
          py::arg("dora_indicators") = std::vector<std::string>{},
          py::arg("visible_tiles_mpsz") = "", py::arg("melds") = py::none(),
          py::arg("t_min") = 1, py::arg("t_max") = 12, py::arg("extra") = 1,
          py::arg("enable_reddora") = true, py::arg("enable_uradora") = false,
          py::arg("enable_shanten_down") = true,
          py::arg("enable_tegawari") = true, py::arg("enable_riichi") = true);
}

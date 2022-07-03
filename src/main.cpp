#include "detfm.hpp"
#include "fmt_swf.hpp"
#include "match/ClassMatcher.hpp"
#include "match/MatchResult.hpp"
#include "renamer.hpp"
#include "utils.hpp"
#include <abc/AbcFile.hpp>
#include <abc/parser/Parser.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <swf/swf.hpp>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#define ARGPARSE_LONG_VERSION_ARG_ONLY
#include <argparse/argparse.hpp>

using namespace swf::abc::parser;
using namespace fmt::literals;
namespace arg = argparse;
namespace fs  = std::filesystem;

template <int slots>
void check_formats(
    Fmt& fmt, YAML::Node& node,
    std::vector<std::pair<std::string, StringFmt<slots> Fmt::*>>&& fields) {
    for (const auto& [name, field] : fields) {
        auto child = node[name];
        if (!child)
            continue;

        if (!child.IsScalar()) {
            utils::log_error("'formats.{}' must be a string.\n", name);
            continue;
        }

        auto value = child.template as<std::string>();
        if (value.empty())
            continue;

        auto error = StringFmt<slots>::valid(value);
        if (error) {
            utils::log_error("'formats.{}' is not a valid format: {}\n", name, *error);
        } else {
            fmt.*field = value;
        }
    }
}

void parse_format(Fmt& fmt, std::string config_file) {
    if (config_file.empty())
        return;

    auto tree = YAML::LoadFile(config_file);
    auto node = tree["formats"];

    if (!node)
        return;

    if (!node.IsMap())
        return utils::log_error(
            "Node 'formats' must be a map {}:{}\n", config_file, node.Mark().line);

    check_formats<1>(
        fmt,
        node,
        {
            { "classes", &Fmt::classes },
            { "consts", &Fmt::consts },
            { "functions", &Fmt::functions },
            { "names", &Fmt::names },
            { "vars", &Fmt::vars },
            { "methods", &Fmt::methods },
            { "errors", &Fmt::errors },
            { "unknown_recv_packet", &Fmt::unknown_recv_packet },
            { "packet_subhandler", &Fmt::packet_subhandler },
        });
    check_formats<2>(
        fmt,
        node,
        {
            { "recv_packet", &Fmt::recv_packet },
            { "sent_packet", &Fmt::sent_packet },
        });
}

uint32_t get_jobs(const uint32_t jobs) {
    if (jobs == 0)
        return std::thread::hardware_concurrency() + 2;
    return jobs;
}

void task(detfm& detfm, std::shared_ptr<abc::AbcFile>& abc, std::deque<abc::Method*>& indexes) {
    static std::mutex mut;
    abc::Method* method = nullptr;
    while (true) {
        {
            std::lock_guard<std::mutex> guard(mut);
            if (indexes.empty())
                break;

            method = indexes.back();
            indexes.pop_back();
        }
        detfm.unscramble(*method);
    }
}

void unscramble(detfm& detfm, std::shared_ptr<abc::AbcFile>& abc, uint32_t jobs) {
    if (jobs == 1)
        return detfm.unscramble();

    std::deque<abc::Method*> indexes;
    const auto length = abc->methods.size();
    for (uint32_t i = 0; i < length; ++i)
        indexes.push_back(&abc->methods[i]);

    utils::log_info("Spawning {} threads.\n", jobs);
    std::vector<std::thread> threads;
    for (int i = 0; i < jobs; ++i)
        threads.emplace_back(task, std::ref(detfm), std::ref(abc), std::ref(indexes));

    for (auto& th : threads)
        th.join();
}

int main(int argc, char const* argv[]) {
    int verbosity = 0;
    arg::ArgumentParser program("detfm", version);
    program.add_description(
        "Deobfuscate Transformice SWF file. The file needs to be unpacked first.");
    program.add_argument("-v", "--verbose")
        .help("Increase output verbosity. Verbose messages go to stderr.")
        .action([&verbosity](const auto& v) { ++verbosity; })
        .append()
        .default_value(0)
        .nargs(0);
    program.add_argument("-j", "--jobs")
        .help("How many threads to spawn in order to do intensive work. A value of 0 will "
              "auto-detect the number of processors available to use.")
        .default_value<uint32_t>(0)
        .scan<'u', uint32_t>();
    program.add_argument("-d", "--classdef")
        .help("Path to a folder containing classes definition (.yaml files).");
    program.add_argument("-c", "--config")
        .help("Specify a config file.")
        .default_value(std::string(""));
    program.add_argument("-C", "--compression")
        .help(
            "Set the compression algorithm for the ouput file. Possible values: none, zlib, lzma.")
        .default_value(std::string("none"))
        .action([](const std::string& value) {
            static const auto choices = { "none", "zlib", "lzma" };
            std::string lower;
            for (auto& c : value)
                lower.push_back(std::tolower(c));

            auto it = std::find(choices.begin(), choices.end(), lower);
            if (it == choices.end())
                throw std::runtime_error("Invalid compression algorithm");

            return *it;
        });
    program.add_argument("input").help("The file to deobfuscate.").required();
    program.add_argument("output").help("The ouput file.").required();

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        utils::log_error("{}\n", err.what());
        utils::log("{}", program.help().str());
        return 1;
    }

    utils::log_level       = verbosity;
    const auto input       = program.get("input");
    const auto output      = program.get("output");
    const auto config      = program.get("--config");
    const auto compression = program.get("--compression");
    const auto jobs        = get_jobs(program.get<uint32_t>("--jobs"));

    utils::TimePoints tps = { { "start", utils::now() } };
    std::unique_ptr<swf::StreamReader> stream;
    std::vector<uint8_t> buffer;

    Fmt fmt;
    parse_format(fmt, config);

    utils::log_info("Reading file '{}'. ", input);
    try {
        if (input == "-") {
            utils::read_from_stdin(buffer);
            stream = std::make_unique<swf::StreamReader>(buffer);
        } else {
            stream = std::unique_ptr<swf::StreamReader>(swf::StreamReader::fromfile(input));
        }
    } catch (const std::runtime_error& err) {
        utils::log("Error: {}\n", err.what());
        return 2;
    }

    utils::log_done(tps, "Reading file");
    utils::log_debug(
        "File size: {}\n",
        utils::fmt_unit({ "B", "kB", "MB", "GB" }, static_cast<double>(stream->size())));
    utils::log_info("Parsing file. ");

    swf::Swf movie;
    movie.read(*stream);

    utils::log_done(tps, "Parsing file");

    auto frame1 = movie.abcfiles.find("frame1");
    if (frame1 == movie.abcfiles.end()) {
        utils::log("Invalid SWF: Frame1 is not available.\n");
        return 2;
    } else {
        utils::log_debug("Found frame1: {}\n", *frame1->second);
        utils::log_info("Renaming invalid fields. ");
    }

    auto abc   = frame1->second->abcfile;
    auto cpool = &abc->cpool;

    try {
        Renamer renamer(abc, fmt);
        renamer.rename();
    } catch (const fmt::format_error& err) {
        utils::log_error("Invalid format: {}", err.what());
        return 2;
    }

    utils::log_done(tps, "Renaming invalid fields");
    utils::log_info("Analyzing methods and classes. ");

    detfm detfm(abc, fmt);
    detfm.analyze();

    utils::log_done(tps, "Analyzing methods and classes");
    utils::log_info("Unscrambling methods.\n");

    unscramble(detfm, abc, jobs);

    utils::log_done(tps, "Unscrambling methods");
    utils::log_info("Renaming interesting stuff. ");

    detfm.rename();

    utils::log_done(tps, "Renaming interesting stuff");
    utils::log_info("Matching user-defined classes.\n");

    if (program.present("--classdef")) {
        std::list<std::shared_ptr<match::ClassMatcher>> classes;

        for (const auto& entry : fs::directory_iterator(program.get("--classdef"))) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                if (path.extension() == ".yml" || path.extension() == ".yaml")
                    load_classdef(path.string(), classes);
            }
        }

        for (auto& klass : classes) {
            bool found = false;
            for (uint32_t i = 0; i < abc->classes.size(); ++i) {
                if (klass->match(abc, i) == match::MatchResult::match) {
                    auto prev = abc->classes[i].get_name();
                    klass->execute_actions();
                    found = true;
                    utils::log_info("Found class: {} -> {}\n", prev, abc->classes[i].get_name());
                    break;
                }
            }

            if (!found)
                utils::log_error("Class not found.\n");

            // some debug shit
            if (!klass->debug.empty()) {
                for (uint32_t i = 0; i < abc->classes.size(); ++i) {
                    if (abc->classes[i].get_name() == klass->debug) {
                        klass->match(abc, i);
                    }
                }
            }
        }
    } else {
        utils::log_info("Skipping, no path given.\n");
    }

    utils::log_done(tps, "Matching user-defined classes");
    utils::log_info("Writing file. ");

    // disable compression by default to speed up the write routine
    movie.signature[0] = static_cast<uint8_t>(
        compression == "none"
            ? swf::Compression::None
            : (compression == "zlib" ? swf::Compression::Zlib : swf::Compression::Lzma));

    swf::StreamWriter writer;
    movie.write(writer);
    if (output == "-") {
        if (std::ferror(std::freopen(nullptr, "wb", stdout))) {
            utils::log_error(std::strerror(errno));
            return 2;
        }

        (void)std::fwrite(writer.get_buffer(), writer.size(), 1, stdout);
    } else {
        writer.tofile(output);
    }
    utils::log_done(tps, "Writing file");

    if (verbosity > 1) {
        utils::log("Timing stats:\n");

        auto it   = tps.begin();
        auto prev = &it->second;

        while (++it != tps.end()) {
            auto took = utils::elapsled(*prev, it->second);
            utils::log(
                " - {action}: {took}\n",
                "action"_a = it->first,
                "took"_a   = utils::fmt_unit({ "µs", "ms", "s" }, took, 1000));
            prev = &it->second;
        }
        auto total = utils::elapsled(tps.front().second, tps.back().second);
        utils::log("Total: {}\n", utils::fmt_unit({ "µs", "ms", "s" }, total, 1000));
    }
    return 0;
}
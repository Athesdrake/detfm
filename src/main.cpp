#include "detfm.hpp"
#include "detfm/common.hpp"
#include "fmt_swf.hpp"
#include "match/ClassMatcher.hpp"
#include "match/MatchResult.hpp"
#include "renamer.hpp"
#include "utils.hpp"
#include <abc/AbcFile.hpp>
#include <abc/parser/Parser.hpp>
#include <algorithm>
#include <argparse/argparse.hpp>
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <fstream>
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
#include <unpacker.hpp>
#include <utility>
#include <variant>
#include <vector>

using namespace swf::abc::parser;
using namespace athes::detfm;
using namespace fmt::literals;
using athes::unpack::Unpacker;
namespace arg   = argparse;
namespace fs    = std::filesystem;
namespace utils = athes::utils;

static utils::Logger logger;

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

    logger.info("Spawning {} threads.\n", jobs);
    std::vector<std::thread> threads;
    for (int i = 0; i < jobs; ++i)
        threads.emplace_back(task, std::ref(detfm), std::ref(abc), std::ref(indexes));

    for (auto& th : threads)
        th.join();
}

auto arg_choices(std::vector<std::string> choices, std::string error_message = "Invalid choice.") {
    return [&](const std::string& value) {
        std::string lower;
        for (auto& c : value)
            lower.push_back(std::tolower(c));

        auto it = std::find(choices.begin(), choices.end(), lower);
        if (it == choices.end())
            throw std::runtime_error(
                fmt::format("{} Valid values are: [{}]", error_message, fmt::join(choices, ", ")));

        return *it;
    };
}

int main(int argc, char const* argv[]) {
    bool enable_proxy = false;
    int verbosity     = 0;
    arg::ArgumentParser program("detfm", version, arg::default_arguments::help);
    program.add_description("Deobfuscate Transformice SWF file.");
    program.add_argument("-V", "--version")
        .action([](const auto&) {
            std::cout << version << std::endl;
            std::exit(0);
        })
        .default_value(false)
        .help("prints version information and exits")
        .implicit_value(true)
        .nargs(0);
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
    program.add_argument("--dump-config")
        .help("Dump the default config file to the specified file.")
        .default_value(std::string(""));
    program.add_argument("--ignore-missing")
        .help("Ignore missing classes and proceed anyway. It will likely crash.")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--no-unpack")
        .help("Don't unpack the swf file before deobfuscating.")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-C", "--compression")
        .help(
            "Set the compression algorithm for the ouput file. Possible values: none, zlib, lzma.")
        .default_value(std::string("none"))
        .action(arg_choices({ "none", "zlib", "lzma" }, "Invalid compression algorithm."));
    program.add_argument("-i")
        .help(
            "The file url to deobfuscate. Can be a file from the filesystem or an url to download.")
        .default_value(std::string { "https://www.transformice.com/Transformice.swf" });
    program.add_argument("-P", "--enable-proxy")
        .help("Change the server's ip to localhost.")
        .action([&enable_proxy](const auto& v) { enable_proxy = true; })
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-p", "--proxy-port")
        .help("Change the server's port to the given value. Implies --enable-proxy")
        .action([&enable_proxy](const auto& v) { enable_proxy = true; })
        .default_value(std::string("11801"));
    program.add_argument("output").help("The ouput file.").required();

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        logger.error("{}\n", err.what());
        logger.log("{}", program.help().str());
        return 1;
    }

    /* verbosity 0  -> warning
                 1  -> info
                 2> -> debug
    */
    logger.level = utils::LogLevel(std::max(3 - verbosity, 1) * 10);

    const auto input       = program.get("-i");
    const auto output      = program.get("output");
    const auto config      = program.get("--config");
    const auto dump_config = program.get("--dump-config");
    const auto compression = program.get("--compression");
    const auto jobs        = get_jobs(program.get<uint32_t>("--jobs"));
    const bool is_url      = input.substr(0, 7) == "http://" || input.substr(0, 8) == "https://";

    utils::TimePoints tps = { { "start", utils::now() } };
    std::unique_ptr<swf::StreamReader> stream;
    std::unique_ptr<Unpacker> unp;
    std::vector<uint8_t> buffer;

    Fmt fmt;

    if (!config.empty()) {
        std::ifstream file(config);
        json cfg = json::parse(file);
        fmt.from_json(cfg, logger);
    }
    if (!dump_config.empty()) {
        std::ofstream file(dump_config);
        file << Fmt::to_json().dump(4);
    }

    auto action = fmt::format("{} file", is_url ? "Downloading" : "Reading");
    logger.info("{} {}. ", action, input);
    try {
        if (is_url) {
            unp = std::make_unique<Unpacker>(input);
        } else if (input == "-") {
            utils::read_from_stdin(buffer);
            stream = std::make_unique<swf::StreamReader>(buffer);
        } else {
            stream = std::unique_ptr<swf::StreamReader>(swf::StreamReader::fromfile(input));
        }
    } catch (const std::runtime_error& err) {
        logger.critical("Error: {}\n", err.what());
        return 2;
    }

    const auto file_size = static_cast<double>(unp ? unp->size() : stream->size());
    logger.log_done(tps, action);
    logger.debug("File size: {}\n", utils::fmt_unit({ "B", "kB", "MB", "GB" }, file_size));

    swf::Swf movie;
    if (!program.get<bool>("--no-unpack")) {
        if (unp == nullptr)
            unp = std::make_unique<Unpacker>(std::move(stream));

        logger.info("Unpacking. ");
        if (!unp->unpack(movie, stream)) {
            logger.log_done(tps, "Unpacking");
            logger.error("Unable to unpack this swf. Is it already unpacked?\n");
        } else {
            logger.log_done(tps, "Unpacking");
        }
    }
    if (movie.file_length == 0) {
        logger.info("Parsing file. ");
        movie.read(*stream);
        logger.log_done(tps, "Parsing file");
    }

    auto frame1 = movie.abcfiles.find("frame1");
    if (frame1 == movie.abcfiles.end()) {
        logger.critical("Invalid SWF: Frame1 is not available.\n");
        return 2;
    } else {
        logger.debug("Found frame1: {}\n", *frame1->second);
        logger.info("Renaming invalid fields. ");
    }

    auto abc   = frame1->second->abcfile;
    auto cpool = &abc->cpool;

    // Rename the symbols to something more readable
    // In fact it's the fully qualified name of a class,
    // so we could rename the symbol using the class' name, but that's not really useful
    uint32_t i = 0;
    for (auto& it : movie.symbol_class->symbols) {
        if (!Renamer::invalid(it.second))
            continue;

        auto pos  = it.second.find('_');
        auto name = '$' + it.second.substr(pos + 1);
        if (Renamer::invalid(name)) {
            // Also rename invalid symbols
            name = fmt.symbols.format(i++);
        }
        std::replace(cpool->strings.begin(), cpool->strings.end(), it.second, name);
        it.second = name;
    }

    // Rename the first class as the Game class
    // Rename the symbol too, so we can Go to document class
    // Not using "Transformice" as the name, since this tool should work on other games too
    // NOTE: FrameLabelTag should be renamed too
    movie.symbol_class->symbols[0] = "Game";
    abc->classes[0].rename("Game");

    try {
        Renamer renamer(abc, fmt);
        renamer.rename();
    } catch (const fmt::format_error& err) {
        logger.error("Invalid format: {}", err.what());
        return 2;
    }

    logger.log_done(tps, "Renaming invalid fields");
    logger.info("Analyzing methods and classes. ");

    detfm detfm(abc, fmt, logger);
    detfm.simplify_init();
    auto missing_classes = detfm.analyze();

    logger.log_done(tps, "Analyzing methods and classes");
    if (!missing_classes.empty()) {
        logger.warn(
            "{} could not be found:\n - {}\n",
            (missing_classes.size() == 1 ? "This class" : "These classes"),
            fmt::join(missing_classes, "\n - "));

        if (!program.get<bool>("--ignore-missing")) {
            logger.error("Use --ignore-missing to ignore this warning and continue.\n"
                         "Continuing will most likely result in a crash.\n");
            return 3;
        }
    }
    logger.info("Unscrambling methods.\n");

    unscramble(detfm, abc, jobs);

    logger.log_done(tps, "Unscrambling methods");
    logger.info("Renaming interesting stuff. ");
    // add a newline when in debug, so logs from rename() are on a new line
    logger.debug("\n");

    detfm.rename();

    logger.log_done(tps, "Renaming interesting stuff");
    logger.info("Matching user-defined classes.\n");

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
                    logger.info("Found class: {} -> {}\n", prev, abc->classes[i].get_name());
                    break;
                }
            }

            if (!found)
                logger.error("Class not found.\n");

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
        logger.info("Skipping, no path given.\n");
    }

    logger.log_done(tps, "Matching user-defined classes");
    if (enable_proxy) {
        const auto port = program.get<std::string>("proxy-port");
        logger.info("Proxying to {}. ", detfm.proxy2localhost(port));
        logger.log_done(tps, "Proxying");
    }
    logger.info("Writing file. ");

    // disable compression by default to speed up the write routine
    movie.signature[0] = static_cast<uint8_t>(
        compression == "none"
            ? swf::Compression::None
            : (compression == "zlib" ? swf::Compression::Zlib : swf::Compression::Lzma));

    swf::StreamWriter writer;
    movie.write(writer);
    if (output == "-") {
        if (std::ferror(std::freopen(nullptr, "wb", stdout))) {
            logger.error(std::strerror(errno));
            return 2;
        }

        (void)std::fwrite(writer.get_buffer(), writer.size(), 1, stdout);
    } else {
        writer.tofile(output);
    }
    logger.log_done(tps, "Writing file");

    if (logger.enabled_for(utils::LogLevel::DEBUG)) {
        logger.debug("Timing stats:\n");

        auto it   = tps.begin();
        auto prev = &it->second;

        while (++it != tps.end()) {
            auto took = utils::elapsled(*prev, it->second);
            logger.debug(
                " - {action}: {took}\n",
                "action"_a = it->first,
                "took"_a   = utils::fmt_unit({ "µs", "ms", "s" }, took, 1000));
            prev = &it->second;
        }
        auto total = utils::elapsled(tps.front().second, tps.back().second);
        logger.debug("Total: {}\n", utils::fmt_unit({ "µs", "ms", "s" }, total, 1000));
    }
    return 0;
}
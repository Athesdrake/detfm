import argparse
import json
import os

MAP_TYPE = "std::unordered_map<const char*, std::string>"
HEADER = """
#include <string>
#include <unordered_map>

namespace pktnames {
""".lstrip()


def validate_packets(packets):
    expected = "Expected packets to be dict[str, str]"
    if not isinstance(packets, dict):
        error_message = f"{expected}, got {type(packets).__name__} instead"
        raise ValueError(error_message)

    for key, value in packets.items():
        if not isinstance(key, str):
            error_message = f"{expected}, got key {key!r} instead"
            raise ValueError(error_message)

        if not isinstance(value, str):
            error_message = f"{expected}, got value {value!r} instead"
            raise ValueError(error_message)

        if '"' in key or '"' in value:
            error_message = f"Packets cannot key and values cannot contains quotes: key={key!r} value={value!r}"
            raise ValueError(error_message)


def main(input_files: list[str], output_file: str):
    with open(output_file, "w") as out:
        out.write(HEADER)
        for file in input_files:
            with open(file, "rb") as pkt_file:
                packets = json.load(pkt_file)
                validate_packets(packets)

            name = os.path.splitext(os.path.basename(file))[0]
            out.write(f"static {MAP_TYPE} {name} = {{\n")
            out.writelines(f'    {{ "{k}", "{v}" }},\n' for k, v in packets.items())
            out.write("};\n\n")

        out.write("}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="+")
    parser.add_argument("-o", "--output", required=1)
    args = parser.parse_args()

    main(args.input, args.output)

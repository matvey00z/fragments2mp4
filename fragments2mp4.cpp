#include <cassert>
#include <string>
#include <memory>
#include <algorithm>
#include <fstream>

#include "meta.pb.h"

#include "bytes.hpp"
#include "mp4.hpp"


struct Input
{
    Input(const std::string &filename)
        : f(filename)
        , r(f)
        , mp4(r)
    {}

    FileReader f;
    Reader r;
    Mp4Reader mp4;
};

struct Chunk
{
    int trak{-1};
    uint64_t offset {0};
};

struct Metadata
{
    std::vector<uint8_t> ftyp;
    std::vector<uint8_t> moov;
    size_t mdat_start {0};
    uint64_t mdat_size {0};
};

std::vector<Chunk> process_moov(std::vector<uint8_t> &moov_data, ssize_t shift)
{
    std::vector<Chunk> chunks;
    VectorReader vector_reader(moov_data);
    Reader reader{vector_reader};
    Mp4Reader moov{reader, moov_data.size()};
    int trak = 0;
    while(!moov.is_last())
    {
        moov.next_box();
        if(moov.get_type() == BoxType("trak"))
        {
            std::vector<BoxType> path{
                BoxType{"mdia"},
                BoxType{"minf"},
                BoxType{"stbl"},
                BoxType{"stco"},
            };
            Mp4Reader stco = moov.down_box().find(path);
            if(stco.get_type() != BoxType("stco"))
                throw "stco not found";
            // TODO search also for 64bit version
            reader.seek(4); // skip version and flags
            uint32_t entry_count = (uint32_t)reader.read_value(4);
            for(uint32_t i = 0; i < entry_count; ++i)
            {
                uint32_t chunk_offset = (uint32_t)reader.read_value(4);
                chunk_offset += shift;
                write_value(&moov_data[reader.get_position() - 4], 4, chunk_offset);
                chunks.push_back(Chunk{trak, chunk_offset});
            }
            ++trak;
        }
    }
    std::sort(std::begin(chunks), std::end(chunks),
            [](Chunk &a, Chunk &b)
            {
                return a.offset < b.offset;
            });
    return chunks;
}

Metadata parse_metadata(const std::string &meta_fname)
{
    std::ifstream meta_f{meta_fname};
    Mp4Metadata::Mp4Metadata proto;
    proto.ParseFromIstream(&meta_f);

    Metadata meta;
    meta.ftyp.resize(proto.ftyp().size());
    memcpy(&meta.ftyp[0], proto.ftyp().data(), meta.ftyp.size());

    meta.moov.resize(proto.moov().size());
    memcpy(&meta.moov[0], proto.moov().data(), meta.moov.size());

    meta.mdat_start = proto.mdat_start();
    meta.mdat_size = proto.mdat_size();

    return meta;
}

struct Args
{
    Args(int argc, char *argv[])
    {
        if(argc < 4)
            usage(argv[0]);
        int argnum = 1;
        output_fname = argv[argnum++];
        meta_fname = argv[argnum++];
        while(argnum < argc)
            input_fnames.push_back(argv[argnum++]);
    }
    static void usage(const char *program)
    {
        fprintf(stderr, "Usage: %s <mp4_filename> <meta_filename> <fmp4_1> [<fmp4_2>...]\n",
                program);
        exit(1);
    }
    std::string output_fname;
    std::string meta_fname;
    std::vector<std::string> input_fnames;
};

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    Args args(argc, argv);
    auto meta = parse_metadata(args.meta_fname);
    FileWriter writer(args.output_fname);
    writer.write(generate_header(BoxType("ftyp"), meta.ftyp.size()));
    writer.write(meta.ftyp);
    writer.write(generate_header(BoxType("moov"), meta.moov.size()));
    size_t mdat_start = writer.get_position() + meta.moov.size();
    auto chunks = process_moov(meta.moov, mdat_start - meta.mdat_start);
    writer.write(meta.moov);
    writer.write(generate_header(BoxType("mdat"), meta.mdat_size));

    std::vector<std::unique_ptr<Input>> inputs;
    for(auto &fname: args.input_fnames)
        inputs.emplace_back(std::make_unique<Input>(fname));

    for(auto &chunk: chunks)
    {
        auto &input = *inputs[chunk.trak];
        do
            input.mp4.next_box();
        while(input.mp4.get_type() != BoxType("mdat"));
        auto mdat = input.mp4.read_data();
        writer.write(mdat);
    }
    return 0;
}

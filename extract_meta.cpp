#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>

#include "meta.pb.h"

#include "bytes.hpp"
#include "mp4.hpp"


struct Metadata
{
    Box ftyp;
    Box moov;
    size_t mdat_start {0};
    uint64_t mdat_size {0};

    bool filled()
    {
        return
            !ftyp.data.empty() &&
            !moov.data.empty() &&
            mdat_start != 0;
    }
};

struct Args
{
    Args(int argc, char *argv[])
    {
        if(argc != 3)
            usage(argv[0]);
        int argnum = 1;
        mp4_fname = argv[argnum++];
        meta_fname = argv[argnum++];
    }
    static void usage(const char *program)
    {
        fprintf(stderr, "Usage: %s <mp4_filename> <metadata_filename>\n", program);
        exit(1);
    }
    std::string mp4_fname;
    std::string meta_fname;
};

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    Args args{argc, argv};
    Metadata metadata;
    try
    {
        FileReader file_reader{args.mp4_fname};
        Reader reader(file_reader);
        Mp4Reader mp4_reader(reader);
        while(!metadata.filled())
        {
            mp4_reader.next_box();
            Box box;
            box.type = mp4_reader.get_type();
            if(box.type == BoxType("ftyp"))
            {
                box.data = mp4_reader.read_data();
                metadata.ftyp = std::move(box);
            }
            else if(box.type == BoxType("moov"))
            {
                box.data = mp4_reader.read_data();
                metadata.moov = std::move(box);
            }
            else if(box.type == BoxType("mdat"))
            {
                metadata.mdat_start = mp4_reader.get_box_start_position();
                metadata.mdat_size = mp4_reader.get_data_size();
            }
        }
    }
    catch(const char *what)
    {
        fprintf(stderr, "Exception while reading input: <%s>\n", what);
        return 1;
    }
    try
    {
        VectorReader vector_reader{metadata.moov.data};
        Reader reader(vector_reader);
        Mp4Reader moov(reader, metadata.moov.data.size());
        int trak_n {0};
        while(!moov.is_last())
        {
            moov.next_box();
            BoxType type = moov.get_type();
            printf("moov.%s\n", type.display().c_str());
            if(type == BoxType("trak"))
            {
                std::vector<BoxType> path{
                    BoxType{"mdia"},
                    BoxType{"minf"},
                    BoxType{"stbl"},
                    BoxType{"stco"},
                };
                Mp4Reader stco = moov.down_box().find(path);
                printf("moov...%s\n", stco.get_type().display().c_str());
                if(stco.get_type() != BoxType("stco"))
                    throw "stco not found";
                // TODO search also for 64bit version
                reader.seek(4); // skip version and flags
                uint32_t entry_count = (uint32_t)reader.read_value(4);
                for(uint32_t i = 0; i < entry_count; ++i)
                    uint32_t chunk_offset = (uint32_t)reader.read_value(4);
                ++trak_n;
            }
        }
    }
    catch(const char *what)
    {
        fprintf(stderr, "Exception while parsing moov: <%s>\n", what);
        return 1;
    }

    Mp4Metadata::Mp4Metadata proto;
    proto.set_ftyp(metadata.ftyp.data.data(), metadata.ftyp.data.size());
    proto.set_moov(metadata.moov.data.data(), metadata.moov.data.size());
    proto.set_mdat_start(metadata.mdat_start);
    proto.set_mdat_size(metadata.mdat_size);
    std::ofstream meta_f{args.meta_fname};
    proto.SerializeToOstream(&meta_f);

    return 0;
}

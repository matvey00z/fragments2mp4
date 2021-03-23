#ifndef MP4_HPP
#define MP4_HPP

#include "bytes.hpp"

inline
bool is_alpha(char c)
{
    return
        ('0' <= c && c <= '9') ||
        ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z');
}

class BoxType
{
    public:
        BoxType()
        {
            memset(box_type, 0, sizeof(box_type) / sizeof(box_type[0]));
        }
        BoxType(const BoxType &other)
        {
            memcpy(box_type, other.box_type, sizeof(box_type));
        }
        BoxType(uint32_t compact)
        {
            memset(box_type, 0, typelen);
            box_type[typelen - 4] = (uint8_t)((compact >> 24) & 0xff);
            box_type[typelen - 3] = (uint8_t)((compact >> 16) & 0xff);
            box_type[typelen - 2] = (uint8_t)((compact >>  8) & 0xff);
            box_type[typelen - 1] = (uint8_t)((compact >>  0) & 0xff);
        }
        BoxType(const std::string &type)
        {
            if(type.size() > typelen)
                throw "Too long box type";
            memset(box_type, 0, typelen);
            memcpy(box_type + typelen - type.size(), type.data(), type.size());
        }
        BoxType(const std::vector<uint8_t> &type)
        {
            if(type.size() > typelen)
                throw "Too long box type";
            memset(box_type, 0, typelen);
            memcpy(box_type + typelen - type.size(), type.data(), type.size());
        }
        friend bool operator==(const BoxType &a, const BoxType &b)
        {
            return !memcmp(a.box_type, b.box_type, sizeof(a.box_type));
        }
        friend bool operator!=(const BoxType &a, const BoxType &b)
        {
            return !(a == b);
        }
        std::string display()
        {
            std::string ret;
            uint8_t *begin = is_compact() ? box_type + 12 : box_type;
            uint8_t *end = box_type + typelen;
            for(uint8_t *i = begin; i != end; ++i)
            {
                char c = (char)*i;
                if(is_alpha(c))
                    ret += c;
                else
                {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%0x", *i);
                    ret += buf;
                }
            }
            return ret;
        }
        bool is_compact()
        {
            for(int i = 0; i < 12; ++i)
                if(box_type[i])
                    return false;
            return true;
        }
        uint32_t get_compact()
        {
            uint32_t ret = 0;
            if(is_compact())
            {
                ret = read_value(box_type + 12, 4);
            }
            else
            {
                ret = (ret << 8) | 'u';
                ret = (ret << 8) | 'u';
                ret = (ret << 8) | 'i';
                ret = (ret << 8) | 'd';
            }
            return ret;
        }
        const uint8_t *get_full()
        {
            return box_type;
        }
    private:
        static constexpr size_t typelen = 16;
        uint8_t box_type[typelen];

};

struct Box
{
    BoxType type;
    std::vector<uint8_t> data;
};


class Mp4Reader
{
    public:
        Mp4Reader(Reader &reader,
                size_t upper_size = 0)
            : reader(reader)
            , reader_start(reader.get_position())
            , box_start{reader_start}
            , box_data_start{reader_start}
            , upper_size(upper_size)
        {
        }
        BoxType get_type()
        {
            return box_type;
        }
        size_t get_data_size()
        {
            return box_data_size;
        }
        void next_box()
        {
            reader.seek_to(box_data_start + box_data_size);
            read_header();
        }
        Mp4Reader down_box()
        {
            return Mp4Reader(reader, get_size_left());
        }
        std::vector<uint8_t> read_data()
        {
            return read_bytes(get_size_left());
        }
        size_t get_box_start_position()
        {
            return box_start;
        }
        bool is_last()
        {
            return reader_start + upper_size == box_data_start + box_data_size;
        }
        Mp4Reader find(const std::vector<BoxType> &path)
        {
            if(path.empty())
                return *this;
            while(get_type() != path[0])
                next_box();
            std::vector<BoxType> cut{std::begin(path) + 1, std::end(path)};
            if(path.size() == 1)
                return *this;
            return down_box().find(cut);
        }
    private:
        Reader &reader;

        size_t reader_start {0};
        size_t box_start {0};
        size_t box_data_start {0};
        size_t box_data_size {0};
        BoxType box_type;

        size_t upper_size{0};

        size_t get_upper_read_size()
        {
            return reader.get_position() - box_start;
        }
        uint64_t read_value(int size)
        {
            if(upper_size && get_upper_read_size() + size > upper_size)
                throw "Read overflow";
            return reader.read_value(size);
        }

        std::vector<uint8_t> read_bytes(size_t size)
        {
            if(upper_size && get_upper_read_size() + size > upper_size)
                throw "Read overflow";
            return reader.read_bytes(size);
        }

        void seek(size_t size)
        {
            if(upper_size && get_upper_read_size() + size > upper_size)
                throw "Read overflow";
            reader.seek(size);
        }

        void read_header()
        {
            box_start = reader.get_position();
            uint64_t box_size = read_value(4);
            box_type = read_value(4);
            if(box_size == 1)
                box_size = read_value(8);
            if(box_type == BoxType("uuid"))
                box_type = read_bytes(16);
            box_data_start = reader.get_position();
            box_data_size = box_start + box_size - box_data_start;
        }
        size_t get_size_left()
        {
            return box_data_start + box_data_size - reader.get_position();
        }
};

inline
std::vector<uint8_t> generate_header(BoxType type, uint64_t data_size)
{
    uint64_t header_size = 0;
    // type
    header_size += 4;
    if(!type.is_compact())
        header_size += 16;
    // size
    header_size += 4;
    if(header_size + data_size > UINT32_MAX)
        header_size += 8;
    size_t total_size = header_size + data_size;
    std::vector<uint8_t> header;
    header.resize(header_size);
    if(total_size <= UINT32_MAX)
        write_value(&header[0], 4, total_size);
    else
    {
        write_value(&header[0], 4, 1);
        write_value(&header[8], 8, total_size);
    }
    write_value(&header[4], 4, type.get_compact());
    if(!type.is_compact())
    {
        size_t pos = (total_size <= UINT32_MAX) ? 8 : 16;
        memcpy(&header[pos], type.get_full(), 16);
    }
    return header;
}

#endif // MP4_HPP

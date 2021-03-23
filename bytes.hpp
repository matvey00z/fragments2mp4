#ifndef BYTES_HPP
#define BYTES_HPP

#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

inline
uint64_t read_value(const uint8_t *bytes, int size)
{
    uint64_t v = 0;
    for(int i = 0; i < size; ++i)
        v = (v << 8) | bytes[i];
    return v;
}

inline
void write_value(uint8_t *dest, int size, uint64_t value)
{
    while(size)
    {
        dest[size - 1] = (uint8_t)(value & 0xff);
        value = value >> 8;
        --size;
    }
}

class ByteReader
{
    public:
        virtual ~ByteReader() = default;
        virtual void read(uint8_t *dest, size_t size) = 0;
        virtual void seek(size_t size) = 0;
        virtual void seek_to(size_t position) = 0;
};

class FileReader: public ByteReader
{
    public:
        FileReader(const std::string &filename)
            : file(fopen(filename.c_str(), "rb"))
        {
            if(!file)
                throw "Can't open file " + filename + " for reading";
        }
        ~FileReader()
        {
            if(file)
                fclose(file);
        }

        virtual void read(uint8_t *dest, size_t size) override
        {
            if(fread(dest, size, 1, file) != 1)
                throw "Can't read from file";
        }

        virtual void seek(size_t size) override
        {
            fseek(file, size, SEEK_CUR);
        }

        virtual void seek_to(size_t position) override
        {
            fseek(file, position, SEEK_SET);
        }
    private:
        FILE *file {nullptr};
};

class VectorReader: public ByteReader
{
    public:
        VectorReader(const std::vector<uint8_t> &data)
            : data(data)
        {}
        virtual void read(uint8_t *dest, size_t size) override
        {
            if(position + size > data.size())
                throw "Overread";
            memcpy(dest, data.data() + position, size);
            position += size;
        }
        virtual void seek(size_t size) override
        {
            position += size;
        }

        virtual void seek_to(size_t new_position) override
        {
            position = new_position;
        }
    private:
        const std::vector<uint8_t> &data;
        size_t position {0};
};

class Reader
{
    public:
        Reader(ByteReader &byte_reader)
            : byte_reader(byte_reader)
        { }
        Reader(const Reader&) = delete;
        Reader(Reader &&) = delete;
        uint64_t read_value(int size)
        {
            std::vector<uint8_t> buf;
            read(buf, size);
            return ::read_value(buf.data(), buf.size());
        }
        std::vector<uint8_t> read_bytes(size_t size)
        {
            std::vector<uint8_t> buf;
            read(buf, size);
            return buf;
        }
        size_t get_position()
        {
            return position;
        }
        void seek(size_t size)
        {
            position += size;
            byte_reader.seek(size);
        }
        void seek_to(size_t new_position)
        {
            position = new_position;
            byte_reader.seek_to(new_position);
        }
    private:
        ByteReader &byte_reader;
        size_t position {0};

        void read(std::vector<uint8_t> &dest, size_t size)
        {
            dest.resize(dest.size() + size);
            read(&dest[dest.size() - size], size);
        }

        void read(uint8_t *dest, size_t size)
        {
            byte_reader.read(dest, size);
            position += size;
        }

};

class FileWriter
{
    public:
        FileWriter(const std::string &filename)
            : f{fopen(filename.c_str(), "wb")}
        {
            if(!f)
                throw "Can't open file for writing";
        }
        ~FileWriter()
        {
            if(f)
                fclose(f);
        }
        void write(const std::vector<uint8_t> &data)
        {
            write(data.data(), data.size());
        }
        void write(const uint8_t *data, size_t size)
        {
            if(fwrite(data, size, 1, f) != 1)
                throw "Error writing file";
        }
        size_t get_position()
        {
            return ftell(f);
        }
    private:
        FILE *f{nullptr};
};


#endif // BYTES_HPP

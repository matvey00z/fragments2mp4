CFLAGS=-g
LDFLAGS=-lprotobuf

all: extract_meta fragments2mp4

meta.pb.{cc,h}: meta.proto
	protoc --cpp_out=. $^

extract_meta: extract_meta.cpp bytes.hpp mp4.hpp meta.pb.{cc,h}
	g++ -o $@ extract_meta.cpp meta.pb.cc $(CFLAGS) $(LDFLAGS)

fragments2mp4: fragments2mp4.cpp bytes.hpp mp4.hpp meta.pb.{cc,h}
	g++ -o $@ fragments2mp4.cpp meta.pb.cc $(CFLAGS) $(LDFLAGS)

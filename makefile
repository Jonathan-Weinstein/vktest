# This probably sucks. I don't normally use make.

CFLAGS := -DVK_NO_PROTOTYPES -std=c++11 -Wall -Wshadow
COMMON_HEADERS := vk_simple_init.h vk_util.h

vktest.out: unity_build.o ext_raster_multisample_test.o  main.o  uav_load_oob.o vk_simple_init.o  vk_util.o clipdistance_tessellation.o xfb_pingpong_bug.o yuy2_r32_copy.o
	g++ *.o -ldl -o vktest.out

unity_build.o: unity_build.cpp
	g++ $(CFLAGS) -c unity_build.cpp

ext_raster_multisample_test.o: ext_raster_multisample_test.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c ext_raster_multisample_test.cpp

clipdistance_tessellation.o: clipdistance_tessellation.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c clipdistance_tessellation.cpp

xfb_pingpong_bug.o: xfb_pingpong_bug.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c xfb_pingpong_bug.cpp

main.o: main.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c main.cpp

uav_load_oob.o: uav_load_oob.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c uav_load_oob.cpp

yuy2_r32_copy.o: yuy2_r32_copy.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c yuy2_r32_copy.cpp

vk_simple_init.o: vk_simple_init.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c vk_simple_init.cpp

vk_util.o: vk_util.cpp $(COMMON_HEADERS)
	g++ $(CFLAGS) -c vk_util.cpp

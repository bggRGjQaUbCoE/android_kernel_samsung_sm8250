export CLANG_PATH=/home/van/clang17
export PATH=${CLANG_PATH}/bin:${PATH}
export CLANG_TRIPLE=aarch64-linux-gnu-
export CROSS_COMPILE=aarch64-linux-gnu-
export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
export CLANG_PREBUILT_BIN=${CLANG_PATH}/bin
export CC="ccache clang"
export CXX="ccache clang++"
export LD=ld.lld
export LLVM=1
export LLVM_IAS=1
export ARCH=arm64
export SUBARCH=arm64

make -j12 O=/home/van/Desktop/out vendor/lineage_y2qdcmw_defconfig

# make -j12 CC=clang O=/home/van/Desktop/out

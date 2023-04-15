config=vendor/lineage_y2qdcmw_defconfig
export PATH="/home/van/clang12/bin:/home/van/GCC_4.9/aarch64-linux-android-4.9/bin:/home/van/GCC_4.9/arm-linux-androideabi-4.9/bin:$PATH"
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
export CROSS_COMPILE_ARM32=arm-linux-android-
export CLANG_TRIPLE=aarch64-linux-gnu-

make -j12 CC=clang O=/home/van/Desktop/out $config

# make -j16 CC=clang O=/home/van/Desktop/out

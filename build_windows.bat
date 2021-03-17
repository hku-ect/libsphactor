set PROJECT_ROOT=%cd%

set INSTALL_PREFIX=%cd%/tmp/ci_build
set LIBZMQ_SOURCEDIR=%cd%/remote/projects/libzmq
set LIBZMQ_BUILDDIR=%LIBZMQ_SOURCEDIR%/build
git clone --depth 1 --quiet https://github.com/zeromq/libzmq.git "%LIBZMQ_SOURCEDIR%"
md "%LIBZMQ_BUILDDIR%"

cd "%LIBZMQ_BUILDDIR%"
cmake .. -DBUILD_STATIC=OFF -DBUILD_SHARED=ON -DZMQ_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%"
cmake --build . --config Debug --target install

set CZMQ_SOURCEDIR=%PROJECT_ROOT%/remote/projects/czmq
set CZMQ_BUILDDIR=%CZMQ_SOURCEDIR%/build
git clone --depth 1 --quiet https://github.com/zeromq/czmq.git "%CZMQ_SOURCEDIR%"
md "%CZMQ_BUILDDIR%"

cd "%CZMQ_BUILDDIR%"
cmake .. -DCZMQ_BUILD_STATIC=OFF -DCZMQ_BUILD_SHARED=ON -DCMAKE_PREFIX_PATH="%INSTALL_PREFIX%" -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%"
cmake --build . --config Debug --target install

set SPH_BUILDDIR="%PROJECT_ROOT%/build"
md "%SPH_BUILDDIR%"
cd "%SPH_BUILDDIR%"
cmake .. -DSPHACTOR_BUILD_STATIC=OFF -DSPHACTOR_BUILD_SHARED=ON -DCMAKE_CXX_FLAGS="-DCZMQ_BUILD_DRAFT_API=1" -DCMAKE_C_FLAGS="-DCZMQ_BUILD_DRAFT_API=1" -DCMAKE_PREFIX_PATH="%INSTALL_PREFIX%" -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" -DCMAKE_IGNORE_PATH="C:/tmp/ci_build"
cmake --build . --config Debug --target install
ctest -C Debug -V

copy "%LIBZMQ_BUILDDIR%\bin\Debug\*.dll" "%SPH_BUILDDIR%\bin\Debug\*.dll"

cd %PROJECT_ROOT%
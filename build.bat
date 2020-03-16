@echo off

pushd bin
call buildsuper_x64.bat ..\4coder_jj_bindings.cpp

copy custom_4coder.dll ..\..\custom_4coder.dll
copy custom_4coder.pdb ..\..\custom_4coder.pdb

popd
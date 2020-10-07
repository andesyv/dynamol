SET LIBRARY_DIRECTORY=%cd%/lib
SET BINARY_DIRECTORY=%cd%/bin

SET DEPENDENCIES=

cd ext

REM FOR %%A IN (glfw,glm,glbinding,globjects) DO CALL :Build %%A
REM FOR %%A IN (glfw,glbinding,globjects) DO CALL :Build %%A
FOR %%A IN (globjects) DO CALL :Build %%A

goto End

:Build
cd "%1"
rd /S /Q build
md build
cd build
cmake .. -DCMAKE_CXX_FLAGS=/D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DCMAKE_INSTALL_PREFIX="%LIBRARY_DIRECTORY%/%1" -DCMAKE_PREFIX_PATH="%DEPENDENCIES%" -DCMAKE_DEBUG_POSTFIX=d -DBUILD_SHARED_LIBS=ON -DGLM_TEST_ENABLE=OFF -DGLFW_BUILD_TESTS=OFF -DOPTION_BUILD_TESTS=OFF -DOPTION_BUILD_EXAMPLES=OFF -DOPTION_BUILD_DOCS=OFF -Dglm_DIR="%LIBRARY_DIRECTORY%/glm/cmake/glm"

cmake --build . --config Debug --target install
cmake --build . --config Release --target install

IF "%DEPENDENCIES%" == "" (
  SET DEPENDENCIES=%LIBRARY_DIRECTORY%/%1
) ELSE (
  SET DEPENDENCIES=%DEPENDENCIES%;%LIBRARY_DIRECTORY%/%1
)

cd ..
cd ..

GOTO :eof

:End
cd %~dp0
rem rd /S /Q ext
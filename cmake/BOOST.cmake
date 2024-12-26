include(FetchContent)

set(BOOST_INCLUDE_LIBRARIES uuid thread program_options headers system filesystem asio regex algorithm serialization locale)

set(BOOST_ENABLE_CMAKE ON)

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_MD5 893b5203b862eb9bbd08553e24ff146a
)
FetchContent_MakeAvailable(Boost)


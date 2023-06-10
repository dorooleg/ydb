FROM ubuntu:18.04
LABEL name="seminar_ydb"

RUN apt-get update -y && apt-get upgrade -y

RUN apt-get -y install wget lsb-release sudo git python2.7 python3.8 gnupg

RUN wget -O llvm-snapshot.gpg.key https://apt.llvm.org/llvm-snapshot.gpg.key
RUN apt-key add llvm-snapshot.gpg.key

RUN wget -O kitware-archive-latest.asc https://apt.kitware.com/keys/kitware-archive-latest.asc
RUN apt-key add kitware-archive-latest.asc

RUN echo "deb http://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/kitware.list >/dev/null
RUN echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-12 main" | tee /etc/apt/sources.list.d/llvm.list >/dev/null

RUN apt-get update

RUN apt-get -y install git cmake python3-pip ninja-build antlr3 m4 clang-12 lld-12 libidn11-dev libaio1 libaio-dev
RUN pip3 install conan==1.59

COPY . ydb/

WORKDIR build

RUN cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../ydb/clang.toolchain ../ydb

RUN ninja tools/actor_model/all

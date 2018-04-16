FROM nginx:1.13

RUN apt-get update && \
	apt-get install -y htop vim curl gnupg2 && \
	curl -sL https://deb.nodesource.com/setup_9.x | bash - && \
	apt-get install -y nodejs && \
	npm install -g yarn

COPY bin/dumb-init_1.2.0_amd64 /dumb-init
RUN chmod +x /dumb-init

RUN apt-get install -y build-essential curl

WORKDIR /cpp-compiler
ADD containers/cpp-compiler/root/cpp-compiler/package.json containers/cpp-compiler/root/cpp-compiler/yarn.lock /cpp-compiler/

RUN yarn install

ADD containers/cpp-compiler/root /

CMD /bin/sh -c 'yarn install && yarn start'
ENTRYPOINT ["/dumb-init", "--"]
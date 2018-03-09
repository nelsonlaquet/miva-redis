FROM node:9.8.0

RUN npm install -g nodemon
ADD containers/cpp-compiler/run.sh /usr/local/bin/run.sh
COPY bin/dumb-init_1.2.0_amd64 /dumb-init

RUN chmod +x /usr/local/bin/run.sh

WORKDIR /miva-redis
CMD /usr/local/bin/run.sh

ENTRYPOINT ["/dumb-init", "--"]
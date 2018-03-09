FROM node:9.8.0

RUN mkdir /watcher
WORKDIR /watcher
ADD containers/miva-compiler/root/watcher/package.json containers/miva-compiler/root/watcher/package-lock.json /watcher/
RUN npm install

ADD containers/miva-compiler/root /

ENV PATH="/msc:${PATH}"
CMD ["start"]
ENTRYPOINT ["npm"]
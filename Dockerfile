FROM hub.deepin.com/library/node:10 AS builder

RUN mkdir /docs
WORKDIR /docs
ADD  ./docs .
RUN npm install -g gitbook-cli
RUN gitbook install ./
RUN gitbook build

FROM hub.deepin.com/library/nginx:latest
COPY --from=builder /docs/_book /usr/share/nginx/html

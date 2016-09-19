unklog: consume kafka log data
==============================

Unklog is a lean log sink for Apache Kafka. It supports pulling logs from Kafka
and outputting them to either [elasticsearch](http://elastic.co) or a long
running process, such as [multilog](https://cr.yp.to/daemontools/multilog.html).

Unklog provides the following features:

- Kafka 0.10.0 compatibility:
  - Balanced consumer with offset storage in kafka.
  - Several topic & cluster support.
- Elasticsearch 2.4 compatibility:
  - Index naming compatible with default kibana approach.
  - Several cluster support.
  - Correct type extraction from logs.
- Statistic reporting
  - Input & output counters
  - Lag reporting
  - Latency histograms

## Configuration

```
log info stderr
input kafka metadata.broker.list=localhost:9092 group.id=unklog-0 topic=logs
output elasticsearch url=http://127.0.0.1:9200
output exec multilog n10 s16384 /var/log/unklog
stats localhost 6789
```

## Statistics

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
global.uptime 1474286538
global.count 11239
in.kafka.count 11239
out.es.count 10640
out.es.errs 0
out.es.lag 599
out.es.meter 10635 0 1 1 2 0 0 0 0 0 0 0 0 max:35
out.exec.count 11239
out.exec.errs 0
out.exec.lag 0
out.exec.meter 11233 0 2 3 1 0 0 0 0 0 0 0 0 max:25
Connection closed by foreign host.
```

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







# HOPPER

# Run Cassandra in Docker

```bash
docker run --name pipercass -p 9042:9042 -d cassandra:latest
```
If you already created pipercass container, you can start it with:
```bash
docker start pipercass
```
if you want to run Cassandra shell to view tables and data, you can run:
```bash
docker exec -it pipercass cqlsh
```
you can view tables with:
```bash
describe tables;
```
if you want to view data in a table, you can run:
```bash
select * from <keyspace>.<table_name>;
```

# unprod steak
2

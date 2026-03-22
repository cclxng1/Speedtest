# Speed Test CLI

Internet speed test program written in C. It finds the best server based on your location and measures download/upload speeds using Ookla speedtest servers.

## How to build

You need gcc, libcurl and cJSON installed. Then just run:

```
make
```

## How to use

Run the full automatic test:
```
./main -e
```

You can also run individual parts:
```
./main -a speed-kaunas.telia.lt:8080    # download test with specific server
./main -b speed-kaunas.telia.lt:8080    # upload test with specific server
./main -c                                # find best server
./main -d                                # detect location
./main -h                                # help
```

## How it works

The program loads a list of ~5800 speedtest servers from a JSON file. When running the full test, it first detects your location using ip-api.com, filters servers by your country, then pings all of them in parallel using curl_multi to find the one with lowest latency. After that it runs download and upload tests against that server using 4 parallel connections and calculates speed in Mbps.

Download and upload tests have a 15 second timeout.

## Files

- main.c - entry point, option parsing
- server.c/h - server loading, filtering, best server selection
- network.c/h - curl operations, location detection, speed tests
- utils.c/h - file reading helper
- Makefile - build script
- speedtest_server_list.json - server list
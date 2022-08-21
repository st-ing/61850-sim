# Fuzzy IEC61850 Simulator

[![Fuzzy IEC61850 Simulator](https://github.com/st-ing/61850-sim/actions/workflows/ci.yml/badge.svg)](https://github.com/st-ing/61850-sim/actions/workflows/ci.yml)


This simulator is a helpful tool that can be used for testing of various [IED](https://en.wikipedia.org/wiki/Intelligent_electronic_device) integration scenarios.
It supports [IEC61850](https://en.wikipedia.org/wiki/IEC_61850) based communication and acts as a [MMS](https://en.wikipedia.org/wiki/Manufacturing_Message_Specification#MMS_stack_over_TCP/IP) server.

Simulation is meant to be run as a (docker) container, as it makes it underlying system agnostic (i.e. laptop, bare-metal, cloud, ...) and brings flexibility when scaling is required (i.e. simulating 200 IEDs).

As an input, the simulation takes a model file that is in any of the [formats](https://en.wikipedia.org/wiki/Substation_Configuration_Language#Types_of_SCL_files) supported by [Substation Configuration Language (SCL)](https://en.wikipedia.org/wiki/Substation_Configuration_Language): ICD (*IED Capability Description*), CID (*Configured IED Description*) or IID (*Instantiated IED Description*).
Based on this model, a runtime is generated dynamically, and simulation is commenced.

## Features

Some of the features that are currently implemented (and some that are 'work in progress'):

* takes ICD, CID, and IID type of model files as input
* applies simulation logic
* configurable simulation frequency
* exposes MMS server endpoint
* configurable MMS port
* configurable logging granularity 
* fuzzification/defuzzification 🚧
* TLS support 🚧
* authentication (password)
* authorization 🚧

## Simulation

The simulation value is calculated based on the combination of the functions ![$f_i = A_i + B_i sin(C_i t + D_i)$](https://latex.codecogs.com/gif.latex?f_i%20%3D%20A_i%20&plus;%20B_i%20sin%28C_i%20t%20&plus;%20D_i%29), where ![$i \in (1..n)$](https://latex.codecogs.com/gif.latex?i%20%5Cin%20%281..n%29) 
but with a salt of 'randomization' for each of the coefficients (![$A_i$](https://latex.codecogs.com/gif.latex?A_i), ![$B_i$](https://latex.codecogs.com/gif.latex?B_i), ![$C_i$](https://latex.codecogs.com/gif.latex?C_i) and ![$D_i$](https://latex.codecogs.com/gif.latex?D_i)) at the time of the simulation initialization, 
and in every simulation iteration for the time horizon parameter (![$t$](https://latex.codecogs.com/gif.latex?t)).

After each time quant (determined by _simulation frequency_ parameter) new simulation values are calculated, and following fuzzification/defuzzification steps, a random data point (from the model) is assigned a new simulation value.

##  Pull it

The simulation (docker) image is automaticaly build and published to (docker) repository and available under
`stinging/61850-sim` and you get it simply by:
```
docker pull stinging/61850-sim
```

Backup/mirror repository is available under `harbor.sting.dev/library/61850-sim` and you get it simply by:
```
docker pull harbor.sting.dev/library/61850-sim
```

##  ... or build it

It is also possible to do a manual build. In order to build the simulator docker image, use the following or similiar commad:
```
docker build -t 61850-sim .
```

## Configure it
The behaviour of the simulator can be configured using environmental variables. The overview is given in the following table:

|Environment Variable|Description|Default value
|--|--|--
| `IED_NAME`        | Name of the IED device             | _IED_ |
| `MMS_PORT`        | IEC61850 MMS server listening port | _102_ |
|_internal_||
| `IEC_61850_EDITION`        | Edition of IEC61850 (1.0, 2.0, 2.1) /respectivly 0, 1, 2/| _1_ |
| `MAX_MMS_CONNECTIONS`        | Maximum number of MMS client connections | _10_ |
| `MAX_DATA_POINTS`             | Modeling logging enabled              | _10000_ |
|_security_||
| `AUTH_PASSWORD`        | Authentication password |  |
|_logging_||
| `LOG_MODELING`             | Modeling logging enabled              | _false_ |
| `LOG_SIMULATION`           | Simulation logging enabled            | _false_ |
| `LOG_DIAGNOSTICS_INTERVAL` | Diagnostics logging interval [**min**] | _5_     |
|_simulation_||
| `SIMULATION_FREQUENCY` | Frequency of signal change (overall) [**Hz**]| _1_ |
|||

The simulation for each individual data point can be additionally configure in **coefficients configuration** file:

```
<?xml version="1.0" encoding="UTF-8"?>
<DataPointsCoefficients>
   ...  
   <DataPoint i="<I>" name="<NAME>" type="<TYPE>">
    ...
    <Coefficient name="<COEFFICIENT>" randomness="<RANDOMNESS>"><VALUE></Coefficient>
    ...
  </DataPoint>
 ...
</DataPointsCoefficients>
```

where *`<I>`* is the unique identifier for the datapoint, *`<NAME>`* is name/path of the data point and *`<TYPE>`* is type;
*`<COEFFICIENT>`* is a coefficient , *`<RANDOMNESS>`* is randomness factor (i.e. `0.1` (10%)) and  *`<VALUE>`* is the value of the coefficient.

The **coefficients configuration** file is (re)generated on every run and can be exposed by mapping - see examples bellow.

## Run it
In order to run the simulation use the following or similiar command:
```
docker run \
    -p 102:102 \
    -v <PATH_TO_SCL_FILE>:/model.cid \
    stinging/61850-sim
```

where *`<PATH_TO_SCL_FILE>`* is path to IED model in any of [ICD, CID, or IID formats](https://en.wikipedia.org/wiki/Substation_Configuration_Language#Types_of_SCL_files).

## Examples

**ABB CoreTec 4**

*(port 102, frequency 10Hz / 0.1s, modeling logging, simulation logging)*

```
docker run -it --rm \
    -p 102:102 \
    -e SIMULATION_FREQUENCY=10 \
    -e LOG_MODELING=true \
    -e LOG_SIMULATION=true \
    -v $(pwd)/res/TEC.scd:/model.cid \
    stinging/61850-sim
```

**Hitachi Energy MSM (Modular Switchgear Monitoring)**

*(port 55555 (on host), specific IED name 'MSM_Test_1', frequency 100Hz / 10ms, coefficient configuration)*

```
docker run -it --rm \
    --network=host \
    -e IED_NAME=MSM_Test_1 \
    -e MMS_PORT=55555 \
    -e SIMULATION_FREQUENCY=100 \
    -v $(pwd)/res/MSM.scd:/model.cid \
    -v $(pwd)/res/MSM.config.xml:/config.xml \
    stinging/61850-sim
```

**Schneider Electric PowerLogic ION7550**

*(port 1000, frequency 1kHz / 1ms, authentication with password 'abcd123', coefficient configuration)*

```
docker run -it --rm \
    -p 1000:102 \
    -e SIMULATION_FREQUENCY=1000 \
    -e LOG_MODELING=true \
    -e AUTH_PASSWORD=abc123 \
    -v $(pwd)/res/ION.icd:/model.cid \
    -v $(pwd)/res/ION.config.xml:/config.xml:ro \
    stinging/61850-sim
```

**Schneider Electric PowerLogic PM8000**

*(frequency 1Hz / 1s, simulation logging)*

```
docker run -it --rm \
    -e SIMULATION_FREQUENCY=1 \
    -e LOG_SIMULATION=true \
    -v $(pwd)/res/PM.icd:/model.cid \
    stinging/61850-sim
```

### As a part of docker compose:

*(specific IED name 'DEV03', frequency 5Hz / 200ms, modeling logging, simulation logging, internal network and specific IP 10.10.0.201)*

```
    iec61850-sim-node-03:
      image: stinging/61850-sim
      container_name: iec61850-sim-node-03
      tty: true
      restart: unless-stopped
      volumes:
        - /opt/61850-sim/DEV.scd:/model.cid
      environment:
        IED_NAME: DEV03
        LOG_MODELING: "true"
        LOG_SIMULATION: "true"
        SIMULATION_FREQUENCY: 5
      networks:
        iec61850-sim-net:
          ipv4_address: 10.10.0.201
```

## Demo servers

- Schneider Electric PowerLogic PM8000 
  
  **server** fuzzy-61850-sim.sting.dev
  
  **port** 1001

  (*docker run -d --name=61850-sim-1001 --restart=unless-stopped -p 1001:102 -e LOG_SIMULATION=true -e SIMULATION_FREQUENCY=1 -v ~/61850-sim-res/PM.icd:/model.cid:ro stinging/61850-sim:1.0*)

- Schneider Electric PowerLogic ION7550

  **server** fuzzy-61850-sim.sting.dev
  
  **port** 1002

  **password** Kr0j@c

  (*docker run -d --name=61850-sim-1002 --restart=unless-stopped -p 1002:102 -e AUTH_PASSWORD=Kr0j@c -e SIMULATION_FREQUENCY=5 -v ~/61850-sim-res/ION.icd:/model.cid:ro stinging/61850-sim:1.0*)

## Third-party components
This code base uses **libIEC61850** library. More documentation can be found online at http://libiec61850.com.

## Future ideas
- introduction of FML (https://en.wikipedia.org/wiki/Fuzzy_markup_language) based simulation definition
- support for Data Sets, Reporting, ... (https://www.beanit.com/iec-61850/tutorial/)
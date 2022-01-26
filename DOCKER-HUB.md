# Quick reference

**Maintained by**: Dusan Stojkovic

**Source code:** https://github.com/st-ing/61850-sim

# Supported tags and respective `Dockerfile` links

- [`latest`, `1.1`](https://github.com/st-ing/61850-sim/blob/1.1/Dockerfile)
- [`1.0`](https://github.com/st-ing/61850-sim/blob/1.0/Dockerfile)

# What is Fuzzy IEC61850 Simulator?

This simulator is a helpful tool that can be used for testing of various [IED](https://en.wikipedia.org/wiki/Intelligent_electronic_device) integration scenarios.
It supports [IEC61850](https://en.wikipedia.org/wiki/IEC_61850) based communication and acts as a [MMS](https://en.wikipedia.org/wiki/Manufacturing_Message_Specification#MMS_stack_over_TCP/IP) server.

Simulation is meant to be run as a (docker) container, as it makes it underlying system agnostic (i.e. laptop, bare-metal, cloud, ...) and brings flexibility when scaling is required (i.e. simulating 200 IEDs).

As an input, the simulation takes a model file that is in any of the [formats](https://en.wikipedia.org/wiki/Substation_Configuration_Language#Types_of_SCL_-) supported by [Substation Configuration Language (SCL)](https://en.wikipedia.org/wiki/Substation_Configuration_Language): ICD (*IED Capability Description*), CID (*Configured IED Description*) or IID (*Instantiated IED Description*).
Based on this model, a runtime is generated dynamically, and simulation is commenced.

More information can be found at https://github.com/st-ing/61850-sim#readme

# How to use this image

## Running
In order to run the simulation use the following or similiar command:

```
docker run \
    -p 102:102 \
    -v <PATH_TO_SCL_FILE>:/model.cid \
    stinging/61850-sim
```

where *`<PATH_TO_SCL_FILE>`* is path to IED model in any of [ICD, CID, or IID formats](https://en.wikipedia.org/wiki/Substation_Configuration_Language#Types_of_SCL_files).



This is the minimal use-case. For more complex use-cases check the *configuring* and *examples*.

## Configuring
The behaviour of the simulator can be configured using environmental variables. The overview is given in the following table:

|Environment Variable|Description|Default value
|--|--|--
| `IED_NAME`        | Name of the IED device             | _IED_ |
| `MMS_PORT`        | IEC61850 MMS server listening port | _102_ |
|_internal_||
| `IEC_61850_EDITION`        | Edition of IEC61850 (1.0, 2.0, 2.1) /respectivly 0, 1, 2/| _1_ |
| `MAX_MMS_CONNECTIONS`        | Maximum number of MMS client connections | _10_ |
| `MAX_DATA_POINTS`             | Modeling logging enabled              | _10000_ |
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

*(port 1000, frequency 1kHz / 1ms, coefficient configuration)*

```
docker run -it --rm \
    -p 1000:102 \
    -e SIMULATION_FREQUENCY=1000 \
    -e LOG_MODELING=true \
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

# Misc
This code base uses **libIEC61850** library. More documentation can be found online at http://libiec61850.com.

The supporting source code is published and available at https://github.com/st-ing/61850-sim.

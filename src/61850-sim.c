#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "static_model.h"

extern IedModel iedModel;

#ifndef MAX_DATA_POINTS
    #define MAX_DATA_POINTS 10000
#endif

#ifndef REPORT_BUFFER_SIZE
    #define REPORT_BUFFER_SIZE 200000
#endif

#ifndef MAX_MMS_CONNECTIONS
    #define MAX_MMS_CONNECTIONS 10
#endif

#ifndef IEC_61850_EDITION
    #define IEC_61850_EDITION IEC_61850_EDITION_2
#endif


  
uint16_t dataPointsCount = MAX_DATA_POINTS;
DataAttribute* dataPointsValues[MAX_DATA_POINTS];
DataAttribute* dataPointsTimestamps[MAX_DATA_POINTS];
DataAttribute* dataPointsQuality[MAX_DATA_POINTS];

float A[MAX_DATA_POINTS], Ar[MAX_DATA_POINTS];
float B[MAX_DATA_POINTS], Br[MAX_DATA_POINTS];
float C[MAX_DATA_POINTS], Cr[MAX_DATA_POINTS];
float D[MAX_DATA_POINTS], Dr[MAX_DATA_POINTS];

static int running = 0;
static IedServer iedServer = NULL;

static uint64_t writeCounter = 0;
static uint64_t readCounter = 0;

void
sigint_handler(int signalId)
{
    running = 0;
}

static void
connectionHandler (IedServer self, ClientConnection connection, bool connected, void* parameter)
{
    if (connected)
    {
        printf("Connection opened - Total connections %d\n",IedServer_getNumberOfOpenConnections(self));
    }
    else
        printf("Connection closed - Total connections %d\n",IedServer_getNumberOfOpenConnections(self));
}

static MmsDataAccessError
readAccessHandler(LogicalDevice* ld, LogicalNode* ln, DataObject* dataObject, FunctionalConstraint fc, ClientConnection connection, void* parameter)
{
    readCounter++;
    return DATA_ACCESS_ERROR_SUCCESS;
}

// (fuzzy) simulation control replacement
float sim(float v, float r) { return v * (1+r*(2.0*rand()/RAND_MAX-1.0)); }

float simA(int i) { return sim(A[i], Ar[i]); }
float simB(int i) { return sim(B[i], Br[i]); }
float simC(int i) { return sim(C[i], Cr[i]); }
float simD(int i) { return sim(D[i], Dr[i]); }
//

void loadCoefficients()
{
    xmlDoc *doc = NULL;
    xmlNode *nodeRoot = NULL;

    LIBXML_TEST_VERSION

    doc = xmlReadFile("/config.xml", NULL, 0);

    if (doc == NULL) return;

    nodeRoot = xmlDocGetRootElement(doc);

    for (xmlNode *nodeDataPoint = nodeRoot->children; nodeDataPoint; nodeDataPoint = nodeDataPoint->next) 
    {
        if (nodeDataPoint->type == XML_ELEMENT_NODE) 
        {
            int i = atoi(xmlGetProp(nodeDataPoint, "i"));
            //printf("%s [%3d]  -  ", xmlGetProp(nodeDataPoint, "name"), i);

            for (xmlNode *nodeCoefficient = nodeDataPoint->children; nodeCoefficient; nodeCoefficient = nodeCoefficient->next) {
                if (nodeCoefficient->type == XML_ELEMENT_NODE) 
                {
                    //printf("  %s = %s ± %s\n", xmlGetProp(nodeCoefficient, "name"), xmlNodeGetContent(nodeCoefficient), xmlGetProp(nodeCoefficient, "randomness"));

                    float X = atof(xmlNodeGetContent(nodeCoefficient));
                    float Xr = atof(xmlGetProp(nodeCoefficient, "randomness"));

                    if (!xmlStrcmp(xmlGetProp(nodeCoefficient, "name"), BAD_CAST "A"))
                    {
                        //printf("   A[%d] = %f ± %0.2f", i, X, Xr);
                        A[i] = X;
                        Ar[i] = Xr;
                    }
                    if (!xmlStrcmp(xmlGetProp(nodeCoefficient, "name"), BAD_CAST "B"))
                    {
                        //printf("   B[%d] = %f ± %0.2f", i, X, Xr);
                        B[i] = X;
                        Br[i] = Xr;
                    }
                    if (!xmlStrcmp(xmlGetProp(nodeCoefficient, "name"), BAD_CAST "C"))
                    {
                        //printf("   C[%d] = %f ± %0.2f", i, X, Xr);
                        C[i] = X;
                        Cr[i] = Xr;
                    }
                    if (!xmlStrcmp(xmlGetProp(nodeCoefficient, "name"), BAD_CAST "D"))
                    {
                        //printf("   D[%d] = %f ± %0.2f", i, X, Xr);
                        D[i] = X;
                        Dr[i] = Xr;
                    }
                }
            }
            //printf("\n");
        }        
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
}

void saveCoefficients()
{
    xmlDocPtr doc = NULL;
    xmlNodePtr nodeRoot = NULL, nodeDataPoint = NULL, nodeCoefficient = NULL;  
    char buff[256];

    LIBXML_TEST_VERSION;

    doc = xmlNewDoc(BAD_CAST "1.0");
    nodeRoot = xmlNewNode(NULL, BAD_CAST "DataPointsCoefficients");
    xmlDocSetRootElement(doc, nodeRoot);

    for (int i = 0; i < dataPointsCount; i++) 
    {
        DataAttribute* dPV = dataPointsValues[i];

        if ((IedModel *)(dPV->parent->parent->parent->parent) != &iedModel)
            sprintf(buff, "%s.%s.%s.%s.%s", dPV->parent->parent->parent->parent->name, dPV->parent->parent->parent->name, dPV->parent->parent->name, dPV->parent->name, dPV->name);
        else
            sprintf(buff, "%s.%s.%s.%s", dPV->parent->parent->parent->name, dPV->parent->parent->name, dPV->parent->name, dPV->name);
        
        
        nodeDataPoint = xmlNewChild(nodeRoot, NULL, BAD_CAST "DataPoint", NULL);
        xmlNewProp(nodeDataPoint, BAD_CAST "name", BAD_CAST buff);      
        sprintf(buff, "%d", i);xmlNewProp(nodeDataPoint, BAD_CAST "i", BAD_CAST buff);

        if (dPV->type == IEC61850_BOOLEAN)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_BOOLEAN");
        if (dPV->type == IEC61850_INT8)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT8");
        if (dPV->type == IEC61850_INT16)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT16");
        if (dPV->type == IEC61850_INT32)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT32");
        if (dPV->type == IEC61850_INT64)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT64");
        if (dPV->type == IEC61850_INT8U)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT8U");
        if (dPV->type == IEC61850_INT16U)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT16U");
        if (dPV->type == IEC61850_INT24U || dPV->type == IEC61850_INT32U)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_INT24/32U");
        if (dPV->type == IEC61850_FLOAT32)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_FLOAT32");
        if (dPV->type == IEC61850_FLOAT64)
            xmlNewProp(nodeDataPoint, BAD_CAST "type", BAD_CAST "IEC61850_FLOAT64");

        sprintf(buff, "%f", A[i]); 
        nodeCoefficient = xmlNewChild(nodeDataPoint, NULL, BAD_CAST "Coefficient", BAD_CAST buff);
        xmlNewProp(nodeCoefficient, BAD_CAST "name", BAD_CAST "A");
        sprintf(buff, "%0.2f", Ar[i]);
        xmlNewProp(nodeCoefficient, BAD_CAST "randomness", BAD_CAST buff);

        sprintf(buff, "%f", B[i]); 
        nodeCoefficient = xmlNewChild(nodeDataPoint, NULL, BAD_CAST "Coefficient", BAD_CAST buff);
        xmlNewProp(nodeCoefficient, BAD_CAST "name", BAD_CAST "B");
        sprintf(buff, "%0.2f", Br[i]);
        xmlNewProp(nodeCoefficient, BAD_CAST "randomness", BAD_CAST buff);

        sprintf(buff, "%f", C[i]); 
        nodeCoefficient = xmlNewChild(nodeDataPoint, NULL, BAD_CAST "Coefficient", BAD_CAST buff);
        xmlNewProp(nodeCoefficient, BAD_CAST "name", BAD_CAST "C");
        sprintf(buff, "%0.2f", Cr[i]);
        xmlNewProp(nodeCoefficient, BAD_CAST "randomness", BAD_CAST buff);

        sprintf(buff, "%f", D[i]); 
        nodeCoefficient = xmlNewChild(nodeDataPoint, NULL, BAD_CAST "Coefficient", BAD_CAST buff);
        xmlNewProp(nodeCoefficient, BAD_CAST "name", BAD_CAST "D");
        sprintf(buff, "%0.2f", Dr[i]);
        xmlNewProp(nodeCoefficient, BAD_CAST "randomness", BAD_CAST buff);
    }

    xmlSaveFormatFileEnc("/config.xml", doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    xmlMemoryDump();
}

int main(int argc, char** argv)
{
    char* ied_name = (getenv("IED_NAME") == NULL) ? "IED" : getenv("IED_NAME");

    int mms_port = (getenv("MMS_PORT") == NULL) ? 102 : atoi(getenv("MMS_PORT"));

    bool log_modeling = (getenv("LOG_MODELING") != NULL) && (strcmp(getenv("LOG_MODELING"), "true") == 0);
    bool log_simulation = (getenv("LOG_SIMULATION") != NULL) && (strcmp(getenv("LOG_SIMULATION"), "true") == 0);
    int simulation_frequency = (getenv("SIMULATION_FREQUENCY") == NULL) ? 1 : atoi(getenv("SIMULATION_FREQUENCY"));    

    int log_diagnostics_interval = (getenv("LOG_DIAGNOSTICS_INTERVAL") == NULL) ? 5 : atoi(getenv("LOG_DIAGNOSTICS_INTERVAL"));

    if (argc > 1)
        ied_name = argv[1];

    if (argc > 2)
        mms_port = atoi(argv[2]);

    printf("Fuzzy IEC61850 Simulation server\n");

    printf("   libIEC61850 version  : %s\n", LibIEC61850_getVersionString());
    printf("   Port                 : %d\n", mms_port);
    printf("   Maximum connections  : %d\n", MAX_MMS_CONNECTIONS);
    printf("   IED Name             : %s\n", ied_name);
    printf("   Modeling log         : %s\n", log_modeling?"true":"false");
    printf("   Simulation log       : %s\n", log_simulation?"true":"false");
    printf("   Simulation frequancy : %d Hz\n", simulation_frequency);
    printf("   Diagnostics interval : %d min\n", log_diagnostics_interval );

    printf("\n");

    // Server configuration
    IedServerConfig config = IedServerConfig_create();
    IedServerConfig_setReportBufferSize(config, REPORT_BUFFER_SIZE);
    IedServerConfig_setEdition(config, IEC_61850_EDITION_2);
    IedServerConfig_setFileServiceBasePath(config, "./vmd-filestore/");
    IedServerConfig_enableFileService(config, false);
    IedServerConfig_enableDynamicDataSetService(config, true);
    IedServerConfig_enableLogService(config, false);
    IedServerConfig_setMaxMmsConnections(config, MAX_MMS_CONNECTIONS);
    IedModel_setIedName(&iedModel, ied_name);

    // New IEC 61850 server instance
    iedServer = IedServer_createWithConfig(&iedModel, NULL, config);
    IedServerConfig_destroy(config);
    IedServer_setServerIdentity(iedServer, "sting GmbH", "Fuzzy IEC61850 Simulator", "1.1");
    
    // Tracking connections
    IedServer_setConnectionIndicationHandler(iedServer, (IedConnectionIndicationHandler) connectionHandler, NULL);

    // Read access handler (only to count reads)
    IedServer_setReadAccessHandler(iedServer, readAccessHandler, NULL);

    // start server
    printf("Starting server... ");
    IedServer_start(iedServer, mms_port);
    if (!IedServer_isRunning(iedServer)) {
        printf("Failed! (maybe need root permissions or another server is already using the port)!\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }
    printf("Done!\n\n");

    running = 1;
    signal(SIGINT, sigint_handler);

    // init coefficients
    dataPointsCount = 0;
    for (int i=0; i<MAX_DATA_POINTS; i++)
    {
        A[i] = NAN; Ar[i] = NAN;
        B[i] = NAN; Br[i] = NAN;
        C[i] = NAN; Cr[i] = NAN;
        D[i] = NAN; Dr[i] = NAN;
    }
    loadCoefficients();

    // runtime prepare
    printf("Browsing the model & preparing runtime... ");

    if (log_modeling) printf("\n");

    LogicalDevice* logicalDevice = iedModel.firstChild;
    while (logicalDevice != NULL)
    {
        if (log_modeling) printf("Logical device - %s\n", logicalDevice->name);

        // pre 

        ModelNode * logicalDeviceChild = logicalDevice->firstChild;
        while (logicalDeviceChild != NULL)
        {
            LogicalNode * logicalNode = (LogicalNode *)logicalDeviceChild;
            if (logicalNode == NULL)
            {
                if (log_modeling) printf("  Logical node ??? %s / %d\n", logicalDeviceChild->name, logicalDeviceChild->modelType);
                logicalDeviceChild = logicalDeviceChild->sibling;
                continue;
            }
            if (log_modeling) printf("  Logical node - %s\n", logicalNode->name);


            // pre 

            ModelNode * logicalNodeChild = logicalNode->firstChild;
            while (logicalNodeChild != NULL)
            {
                DataObject * dataObject = (DataObject *)logicalNodeChild;
                if (dataObject == NULL)
                {
                    if (log_modeling) printf("    Data object ??? %s / %d\n", logicalNodeChild->name, logicalNodeChild->modelType);
                    logicalNodeChild = logicalNodeChild->sibling;
                    continue;
                }
                if (log_modeling) printf("    Data object - %s\n", dataObject->name);

                // pre 
                DataAttribute * dA_VAL = NULL;
                DataAttribute * dA_TS = NULL;
                DataAttribute * dA_Q = NULL;

                // iterate
                ModelNode * dataObjectChild = dataObject->firstChild;
                while (dataObjectChild != NULL)
                {
                    if (dataObjectChild->modelType == DataObjectModelType)
                    {
                        DataObject * dO = (DataObject *)dataObjectChild;
                        if (log_modeling) printf("      Data object - %s ???\n", dO->name);
                        
                        dataObjectChild = dataObjectChild->sibling;
                        continue;
                    }

                    DataAttribute * dA = (DataAttribute *)dataObjectChild;
                    if (dataObjectChild->modelType != DataAttributeModelType || dA == NULL)
                    {
                        if (log_modeling) printf("      ??? Model Type '%d' - %s\n", dataObjectChild->modelType, dataObjectChild->name);
                        dataObjectChild = dataObjectChild->sibling;
                        continue;
                    }
                    if (log_modeling) printf("      Data attribute - %s (#%d) %d", dA->name, dA->sAddr, dA->type);

                    //////

                    if (dA->type == IEC61850_CONSTRUCTED) if (log_modeling) printf(" /IEC61850_CONSTRUCTED");
                    if (dA->type == IEC61850_TIMESTAMP) if (log_modeling) printf(" /IEC61850_TIMESTAMP");
                    if (dA->type == IEC61850_QUALITY) if (log_modeling) printf(" /IEC61850_QUALITY");
                    if (dA->triggerOptions & TRG_OPT_DATA_CHANGED) if (log_modeling) printf(" *TRG_OPT_DATA_CHANGED");
                    if (dA->triggerOptions & TRG_OPT_DATA_UPDATE) if (log_modeling) printf(" *TRG_OPT_DATA_UPDATE");
                    
                    if (dA->type == IEC61850_TIMESTAMP)
                    {
                        dA_TS = dA;
                        if (log_modeling) printf(" [IEC61850_TIMESTAMP]");
                    }
                    if (dA->type == IEC61850_QUALITY)
                    {
                        dA_Q = dA;
                        if (log_modeling) printf(" [IEC61850_QUALITY]");
                    }

                    DataAttribute * dP = dA;
                    if (dA->type == IEC61850_CONSTRUCTED)
                    {
                        dP = (DataAttribute * )(dP->firstChild);
                        if (log_modeling) printf("\n        Data attribute - %s %d", dP->name, dP->type);
                    }

                    if ((dA->triggerOptions & TRG_OPT_DATA_CHANGED) || (dA->triggerOptions & TRG_OPT_DATA_UPDATE))
                    {
                        // default coeficients
                        int i = dataPointsCount;

                        if (dP->type == IEC61850_BOOLEAN)
                        {
                            if isnan(B[i]) { B[i] = 1.0f; Br[i] = 0.01f; }
                            
                            if (log_modeling) printf(" [IEC61850_BOOLEAN]");
                            dA_VAL = dP;
                        }
                        if (dP->type == IEC61850_INT8)
                        {
                            if isnan(B[i]) { B[i] = 0.95f * INT8_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT8]");
                            dA_VAL= dP;                            
                        }
                        if (dP->type == IEC61850_INT16)
                        {
                            if isnan(B[i]) { B[i] = 0.95f * INT16_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT16]");
                            dA_VAL= dP;                            
                        }
                        if (dP->type == IEC61850_INT32)
                        {
                            if isnan(B[i]) { B[i] = 0.95f * INT32_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT32]");
                            dA_VAL= dP;
                        }
                        if (dP->type == IEC61850_INT64)
                        {       
                            if isnan(B[i]) { B[i] = 0.95f * INT64_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT64]");
                            int64_t z = 0;
                            dA_VAL = dP;
                        }
                        if (dP->type == IEC61850_INT8U)
                        {
                            if isnan(A[i]) { A[i] = 0.5f * INT8_MAX; Br[i] = 0.00f; }
                            if isnan(B[i]) { B[i] = 0.45f * INT8_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT8U]");
                            uint8_t t = 0;
                            dA_VAL = dP;
                        }
                        if (dP->type == IEC61850_INT16U)
                        {
                            if isnan(A[i]) { A[i] = 0.5f * INT16_MAX; Br[i] = 0.00f; }
                            if isnan(B[i]) { B[i] = 0.45f * INT16_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT16U]");
                            dA_VAL = dP;                            
                        }
                        if (dP->type == IEC61850_INT24U ||
                            dP->type == IEC61850_INT32U)
                        {
                            if isnan(A[i]) { A[i] = 0.5f * INT32_MAX; Br[i] = 0.00f; }
                            if isnan(B[i]) { B[i] = 0.45f * INT32_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_INT24/32U]");
                            dA_VAL= dP;
                        }
                        if (dP->type == IEC61850_FLOAT32)
                        {
                            if isnan(B[i]) { B[i] = 0.95f * FLT_MAX; Br[i] = 0.05f; }

                            if (log_modeling) printf(" [IEC61850_FLOAT32]");
                            dA_VAL = dP;
                        }
                        if (dP->type == IEC61850_FLOAT64)
                        {
                            if isnan(B[i]) { B[i] = 0.95f * DBL_MAX; Br[i] = 0.05f;}

                            if (log_modeling) printf(" [IEC61850_FLOAT64]");
                            dA_VAL = dP;
                        }
                        
                        if isnan(A[i]) { A[i] = 0.0f; Ar[i] = 0.01f; }
                        if isnan(B[i]) { B[i] = 1.0f; Br[i] = 0.01f; }
                        if isnan(C[i]) { C[i] = sim(1,0.8); Cr[i] = 0.01f; }          // time 0.2..1.8 randomness 1%
                        if isnan(D[i]) { D[i] = sim(M_PI, 1.0); Dr[i] = 0.1f; }       // phase 0..2*PI randomness 10%

                        if (log_modeling) printf("   A: %f ± %0.0f%%   B: %f ± %0.0f%%   C: %f ± %0.0f%%   D: %f ± %0.0f%%", A[i], 100*Ar[i], B[i], 100*Br[i], C[i], 100*Cr[i], D[i], 100*Dr[i] );
                    }
                                       
                    if (log_modeling) printf("\n");
                    /////

                    dataObjectChild = dataObjectChild->sibling;
                }

                // post
                if (dA_TS != NULL && dA_VAL != NULL)
                {
                    dataPointsValues[dataPointsCount] = dA_VAL;
                    dataPointsTimestamps[dataPointsCount] = dA_TS;
                    dataPointsQuality[dataPointsCount] = dA_Q;
                    dataPointsCount++;
                    if (log_modeling) printf("      --- %d  ---\n", dataPointsCount);

                    if (dataPointsCount == MAX_DATA_POINTS)
                    {
                        printf("Error - Maximum number (%d) of data points reached. Increase parameter MAX_DATA_POINTS. Terminating...", MAX_DATA_POINTS);
                        exit(EXIT_FAILURE);
                    }
                }                
                logicalNodeChild = logicalNodeChild->sibling;
            }

            // post

            logicalDeviceChild = logicalDeviceChild->sibling;
        }

        // post

        logicalDevice = (LogicalDevice *)(logicalDevice->sibling);
    }
    printf("Done!\n\n");
    
    saveCoefficients();

    // runtime
    printf("Starting simulation...\n");

    float t = 0.f;
    srandom(Hal_getTimeInMs());

    uint64_t timestamp_ = Hal_getTimeInMs();

    while (running) {
        uint64_t timestamp = Hal_getTimeInMs();

        t += 1.0f / simulation_frequency;

        Timestamp iecTimestamp;
        Timestamp_clearFlags(&iecTimestamp);
        Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
        Timestamp_setLeapSecondKnown(&iecTimestamp, true);

        Quality iecQuality = QUALITY_VALIDITY_GOOD;

        /* toggle clock-not-synchronized flag in timestamp */
        if (((int) t % 2) == 0)
            Timestamp_setClockNotSynchronized(&iecTimestamp, true);

        IedServer_lockDataModel(iedServer);

        int i = random() * dataPointsCount / RAND_MAX;

        DataAttribute* dPT = dataPointsTimestamps[i];
        DataAttribute* dPV = dataPointsValues[i];
        DataAttribute* dPQ = dataPointsQuality[i];

        if (dPT == NULL || dPV == NULL) continue;
        
        if (log_simulation)
        {
            if ((IedModel *)(dPV->parent->parent->parent->parent) != &iedModel)
                printf("%s.", dPV->parent->parent->parent->parent->name);
            printf("%s.%s.%s.", dPV->parent->parent->parent->name, dPV->parent->parent->name, dPV->parent->name);
        }

        float simVal = simA(i) + simB(i) * sinf( simC(i) * t + simD(i));
        
        if (dPV->type == IEC61850_FLOAT32 ||
            dPV->type == IEC61850_FLOAT64)
        {
            float val = simVal;
            if (log_simulation) printf("%s [FLOAT] <- %f\n", dPV->name, val);
            IedServer_updateTimestampAttributeValue(iedServer, dPT, &iecTimestamp);
            IedServer_updateQuality(iedServer, dPQ, iecQuality);
            IedServer_updateFloatAttributeValue(iedServer, dPV, val);
            writeCounter++;
        } else
        if (dPV->type == IEC61850_INT8 ||
            dPV->type == IEC61850_INT16 ||
            dPV->type == IEC61850_INT32)
        {
            int32_t val = simVal;
            if (log_simulation) printf("%s [INT]  <- %d\n", dPV->name, val);
            IedServer_updateTimestampAttributeValue(iedServer, dPT, &iecTimestamp);
            IedServer_updateQuality(iedServer, dPQ, iecQuality);
            IedServer_updateInt32AttributeValue(iedServer, dPV, val);
            writeCounter++;
        } else
        if (dPV->type == IEC61850_INT64)
        {
            int64_t val = simVal;
            if (log_simulation) printf("%s [LONG] <- %ld\n", dPV->name, val);
            IedServer_updateTimestampAttributeValue(iedServer, dPT, &iecTimestamp);
            IedServer_updateQuality(iedServer, dPQ, iecQuality);
            IedServer_updateFloatAttributeValue(iedServer, dPV, val);
            writeCounter++;
        } else            
        if (dPV->type == IEC61850_INT8U ||
            dPV->type == IEC61850_INT16U ||
            dPV->type == IEC61850_INT24U ||
            dPV->type == IEC61850_INT32U)
        {
            uint32_t val = abs(simVal);
            if (log_simulation) printf("%s [UINT] <- %d\n", dPV->name, val);
            IedServer_updateTimestampAttributeValue(iedServer, dPT, &iecTimestamp);
            IedServer_updateQuality(iedServer, dPQ, iecQuality);
            IedServer_updateUnsignedAttributeValue(iedServer, dPV, val);
            writeCounter++;
        } else
        if (dPV->type == IEC61850_BOOLEAN)
        {
            bool val = simVal >= 0.0f;
            if (log_simulation) printf("%s [BOOL] <- %s\n", dPV->name, val ? "true" : "false");
            IedServer_updateTimestampAttributeValue(iedServer, dPT, &iecTimestamp);
            IedServer_updateQuality(iedServer, dPQ, iecQuality);
            IedServer_updateBooleanAttributeValue(iedServer, dPV, val);
            writeCounter++;
        }

        IedServer_unlockDataModel(iedServer);


        if (((timestamp/1000) % (60 * log_diagnostics_interval)) == 0 && timestamp-timestamp_ > 1000) // every 15 minutes
        {
            printf(" [%ld] total simulated / read (last %d s) - %.1f/s / %.1f/s\n", timestamp / 1000, (int)(timestamp-timestamp_) / 1000, 1000.0f * writeCounter / (timestamp-timestamp_), 1000.0f * readCounter / (timestamp-timestamp_));
            timestamp_ = timestamp;
            writeCounter = 0;
            readCounter = 0;
        }

        //uint64_t sleeptime = Hal_getTimeInMs() - timestamp + 1000.0f / simulation_frequency;
        //Thread_sleep(sleeptime);

        Thread_sleep(1000.0f / simulation_frequency);
    }

    printf("Stopped!\n\n");

    // stop server, close TCP server and client sockers
    IedServer_stop(iedServer);

    // cleanup / free resources
    IedServer_destroy(iedServer);

}

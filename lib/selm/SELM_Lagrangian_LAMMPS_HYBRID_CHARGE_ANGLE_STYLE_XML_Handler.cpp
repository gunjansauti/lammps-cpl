/*

  SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler.cpp

  http://atzberger.org/
 
*/

#include "SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace LAMMPS_NS {

SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler() {

  setupDataHandler();

  lagrangian = NULL;

}

SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler(SELM_Lagrangian_Delegator_XML_Handler *delegatorHandler) {

  setupDataHandler();

  lagrangian = new SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE();

  strcpy(lagrangian->nameStr, delegatorHandler->SELM_LagrangianName);
  strcpy(lagrangian->typeStr, delegatorHandler->SELM_LagrangianTypeStr);

}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::setupDataHandler() {

  DataHandlerName            = "Data Handler for SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler";
  DataHandlerType            = "SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler";

  xmlTagName_xml             = "xml";
  xmlTagName_LagrangianName  = "LagrangianName";
  xmlTagName_SELM_Lagrangian = "SELM_Lagrangian";
  xmlTagName_num_dim         = "num_dim";
  xmlTagName_numControlPts   = "numControlPts";
  xmlTagName_ptsX            = "ptsX";
  xmlTagName_atomID          = "atomID";
  xmlTagName_moleculeID      = "moleculeID";
  xmlTagName_typeID          = "typeID";
  xmlTagName_atomMass        = "atomMass";
  xmlTagName_atomCharge      = "atomCharge";
  xmlTagName_pt_Vel          = "pt_Vel";
  xmlTagName_pt_Energy       = "pt_Energy";
  xmlTagName_pt_Force        = "pt_Force";
  xmlTagName_pt_type         = "pt_type";
  xmlTagName_pt_type_extras  = "pt_type_extras";
  xmlTagName_flagWriteVTK    = "flagWriteVTK";

}


SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::~SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler() {
  // TODO Auto-generated destructor stub
}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_startDocument(Atz_XML_SAX_DataHandler* sourceHandler) {

}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_endDocument(Atz_XML_SAX_DataHandler* sourceHandler) {

}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_startElement(string qName, Atz_XML::AttributesType *attributes, Atz_XML_SAX_DataHandler* sourceHandler) {

  xmlAttributes = attributes;

  xmlString.clear();

  if (qName == xmlTagName_xml) {

  } else if (qName == xmlTagName_SELM_Lagrangian) {

    /* setup the data structure */
    lagrangian = new SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE();

  } else if (qName == xmlTagName_LagrangianName) {

  } else if (qName == xmlTagName_num_dim) {

  } else if (qName == xmlTagName_numControlPts) {

  } else if (qName == xmlTagName_ptsX) {

  } else if (qName == xmlTagName_atomID) {

  } else if (qName == xmlTagName_moleculeID) {

  } else if (qName == xmlTagName_typeID) {

  } else if (qName == xmlTagName_atomMass) {

  } else if (qName == xmlTagName_atomCharge) {

  } else if (qName == xmlTagName_pt_Vel) {

  } else if (qName == xmlTagName_pt_type) {

  } else if (qName == xmlTagName_pt_type_extras) {

  } else if (qName == xmlTagName_flagWriteVTK) {

  } else if (qName == xmlTagName_flagMobile) {

  } else { /* unrecognized tags skip (avoid sub-tags triggering something here) */
    Atz_XML_SAX_Handler_Multilevel* sourceHandler_Multilevel;
    Atz_XML_SAX_DataHandler* dataHandler;

    sourceHandler_Multilevel = dynamic_cast<Atz_XML_SAX_Handler_Multilevel*> (sourceHandler);
    dataHandler = new Atz_XML_Helper_Handler_SkipNextTag();
    sourceHandler_Multilevel->parseNextTagWithDataHandler(dataHandler);

  }

}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_characters(string xmlString_in, Atz_XML_SAX_DataHandler* sourceHandler) {
  xmlString += xmlString_in;
}

void SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_endElement(string qName, Atz_XML_SAX_DataHandler* sourceHandler) {

  if (qName == xmlTagName_LagrangianName) {

    strcpy(lagrangian->nameStr, Atz_XML_Helper_ParseData::getStringFromAttr(xmlAttributes)->c_str());

  } else if (qName == xmlTagName_num_dim) {

    lagrangian->num_dim = Atz_XML_Helper_ParseData::getIntFromAttr(xmlAttributes);

  } else if (qName == xmlTagName_numControlPts) {

    //lagrangian->numControlPts       = Atz_XML_Helper_ParseData::getIntFromAttr(xmlAttributes);
    //lagrangian->numControlPts_alloc = lagrangian->numControlPts;

  } else if (qName == xmlTagName_ptsX) {

    int N;

    if (lagrangian->ptsX != NULL) {
      free(lagrangian->ptsX);
    }

    lagrangian->ptsX = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseDoubleArrayFromString(xmlString, &lagrangian->ptsX, &N);

    lagrangian->numControlPts       = N/lagrangian->num_dim;
    //lagrangian->numControlPts_alloc = N/lagrangian->num_dim;

    if (lagrangian->pt_Vel == NULL) {
      lagrangian->pt_Vel = (double *)malloc(sizeof(double)*N);
    }

    if (lagrangian->pt_Force == NULL) {
      lagrangian->pt_Force = (double *)malloc(sizeof(double)*N);
    }

  } else if (qName == xmlTagName_atomID) {

    int N;
    lagrangian->atomID = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseIntArrayFromString(xmlString, &lagrangian->atomID, &N);

  } else if (qName == xmlTagName_moleculeID) {

    int N;
    lagrangian->moleculeID = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseIntArrayFromString(xmlString, &lagrangian->moleculeID, &N);


  } else if (qName == xmlTagName_typeID) {

    int N;
    lagrangian->typeID = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseIntArrayFromString(xmlString, &lagrangian->typeID, &N);

  } else if (qName == xmlTagName_atomMass) {

    int N;
    lagrangian->atomMass = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseDoubleArrayFromString(xmlString, &lagrangian->atomMass, &N);

  } else if (qName == xmlTagName_atomCharge) {

    int N;
    lagrangian->atomCharge = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseDoubleArrayFromString(xmlString, &lagrangian->atomCharge, &N);

  } else if (qName == xmlTagName_pt_Vel) {

    int N;

    if (lagrangian->pt_Vel != NULL) {
      free(lagrangian->pt_Vel);
    }

    lagrangian->pt_Vel = NULL; /* indicates parse routine should allocate array */
    Atz_XML_Helper_ParseData::parseDoubleArrayFromString(xmlString, &lagrangian->pt_Vel, &N);

  } else if (qName == xmlTagName_pt_type) {

    /* parse list from string of characters */

  } else if (qName == xmlTagName_pt_type_extras) {

    /* type specific data... */

  } else if (qName == xmlTagName_flagWriteVTK) {

    lagrangian->flagWriteVTK = Atz_XML_Helper_ParseData::getIntFromAttr(xmlAttributes);
  } else if (qName == xmlTagName_flagMobile) {

    lagrangian->flagMobile = Atz_XML_Helper_ParseData::getIntFromAttr(xmlAttributes);

  } else { /* unrecognized tags skip (avoid sub-tags triggering something here) */

  }

}

void *SELM_Lagrangian_LAMMPS_HYBRID_CHARGE_ANGLE_STYLE_XML_Handler::XML_getData() { /* gets data from parsing the XML */
  return lagrangian;
}



}

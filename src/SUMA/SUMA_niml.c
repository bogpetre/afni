#include "SUMA_suma.h"

/**************************************/
/** global data for NIML connections **/
/**************************************/
extern SUMA_SurfaceViewer *SUMAg_cSV;
extern int SUMAg_N_DOv; 
extern SUMA_DO *SUMAg_DOv;
extern SUMA_CommonFields *SUMAg_CF; 

/*-----------------------------------------------*/
/*! Flag to tell if NIML things are initialized. */

static int started = 0 ;


/*-----------------------------------------------------------------------*/
/*! NIML workprocess.
    - Read and process any new data from open connection.

  (If the return is True, that means don't call this workproc again.
   If the return is False, that means call this workproc again.......)
-------------------------------------------------------------------------*/

Boolean SUMA_niml_workproc( XtPointer thereiselvis )
{
   int cc , nn ;
   void *nini ;
	static char FuncName[]={"SUMA_niml_workproc"};
	SUMA_Boolean LocalHead = NOPE;
	
	if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

     /* check if stream is open */

     if( SUMAg_cSV->ns == NULL ){
       fprintf(SUMA_STDERR,"Error SUMA_niml_workproc: Stream is not open. \n");
		 SUMA_RETURN(True); /* Don't call me with that lousy stream again */
     }

     /* check if stream has gone bad */

     nn = NI_stream_goodcheck( SUMAg_cSV->ns , 1 ) ;

     if( nn < 0 ){                          /* is bad */
       NI_stream_close( SUMAg_cSV->ns ) ;
       SUMAg_cSV->ns = NULL ;
		 fprintf(SUMA_STDERR,"Error SUMA_niml_workproc: Stream gone bad. Stream closed. \n");
       SUMA_RETURN(True);               /* Don't call me with that lousy stream again */
     }

     /* if here, stream is good;
        see if there is any data to be read */

	#if 0
		/* not good enough, checks socket only, not buffer */
		nn = NI_stream_readcheck( SUMAg_cSV->ns , 1 ) ;
	#else
		nn = NI_stream_hasinput( SUMAg_cSV->ns , 1 ) ;
	#endif
	
     if( nn > 0 ){                                   /* has data */
       int ct = NI_clock_time() ;
		 if (LocalHead)	fprintf(SUMA_STDERR,"$s: reading data stream", FuncName) ;

       nini = NI_read_element( SUMAg_cSV->ns , 1 ) ;  /* read it */

		if (LocalHead)	fprintf(SUMA_STDERR," time=%d ms\n",NI_clock_time()-ct) ; ct = NI_clock_time() ;

       if( nini != NULL ) {
       	if (LocalHead)	{
				NI_element *nel ;
				nel = (NI_element *)nini ;
				fprintf(SUMA_STDERR,"%s:     name=%s vec_len=%d vec_filled=%d, vec_num=%d\n", FuncName,\
						nel->name, nel->vec_len, nel->vec_filled, nel->vec_num );
			}		
		    if (!SUMA_process_NIML_data( nini )) {
			 	fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_process_NIML_data.\n", FuncName);
			 }
		}

      NI_free_element( nini ) ;

		if (LocalHead)	fprintf(SUMA_STDERR,"processing time=%d ms\n",NI_clock_time()-ct) ;

     }
   

   SUMA_RETURN(False) ;  /* always call me back */
}

/*----------------------------------------------------------------------*/
/*! Process NIML data.  
------------------------------------------------------------------------*/

SUMA_Boolean SUMA_process_NIML_data( void *nini )
{
	int tt = NI_element_type(nini) ;
   NI_element *nel ;
	SUMA_EngineData EngineData; /* Do not free EngineData, only its contents*/
	char CommString[SUMA_MAX_COMMAND_LENGTH], *nel_surfidcode;
	char s[SUMA_MAX_STRING_LENGTH], sfield[100], sdestination[100], ssource[100];
	float **fm, dimfact,  *XYZ;
	int i, *inel, I_C = -1, iv3[3];
	byte *r, *g, *b;
	static char FuncName[]={"SUMA_process_NIML_data"};
	SUMA_Boolean Empty_irgba = NOPE, LocalHead = NOPE;
	
	SUMA_SurfaceObject *SO;
	/*int it;
	float fv3[3], fv15[15];*/
	/*float ft;
	int **im,  iv15[15];*/ /* keep unused variables undeclared to quite compiler */

	if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

  	/* retrieve the Surface Object in Focus*/
		SO = (SUMA_SurfaceObject *)(SUMAg_DOv[SUMAg_cSV->Focus_SO_ID].OP);
		

	/* initialize EngineData */
	if (!SUMA_InitializeEngineData (&EngineData)) {
		fprintf(SUMA_STDERR,"Error %s: Failed to initialize EngineData\n", FuncName);
		SUMA_RETURN(NOPE);
	}

   if( tt < 0 ) {/* should never happen */
		fprintf(SUMA_STDERR,"Error %s: Should never have happened.\n", FuncName);
		SUMA_RETURN(NOPE);
	} 

   if( tt != NI_ELEMENT_TYPE ){  /* should never happen */
		fprintf(SUMA_STDERR,"Error %s: Should never have happened.\n", FuncName);
		SUMA_RETURN(NOPE);
	}
   
	/* if here, have a single data element;
      process the data based on the element name */

   nel = (NI_element *) nini ;
	
	if (LocalHead)  {
		fprintf(SUMA_STDERR,"%s:     name=%s vec_len=%d vec_filled=%d, vec_num=%d\n", FuncName,\
					nel->name, nel->vec_len, nel->vec_filled, nel->vec_num );
	}
	
   /*--- CrossHair XYZ ---*/
	if( strcmp(nel->name,"SUMA_crosshair_xyz") == 0) {/* SUMA_crosshair_xyz */
     /*-- check element for suitability --*/
     if( nel->vec_len    < 1 || nel->vec_filled <  1) {  /* empty element?             */
			fprintf(SUMA_STDERR,"%s: Empty crosshair xyz.\n", FuncName);
	  		SUMA_RETURN(YUP);
	  }
	  if( nel->vec_len != 3 || nel->vec_num != 1 || nel->vec_typ[0] != NI_FLOAT) {
	  		fprintf(SUMA_STDERR,"%s: SUMA_crosshair_xyz requires 3 floats in one vector.\n", FuncName);
			SUMA_RETURN(NOPE);
	  }

		/*SUMA_nel_stdout (nel);*/

  	   /* set the cross hair XYZ for now */
	   I_C = -1;
	   XYZ = SUMA_XYZmap_XYZ (nel->vec[0], SO, SUMAg_DOv, SUMAg_N_DOv, &I_C);
	  
	  if (XYZ == NULL) {
	  	fprintf(SUMA_STDERR,"Error %s: No linkage possible.\n", FuncName);
		SUMA_RETURN(NOPE);
	  }

		/* attach the cross hair to the selected surface */
		nel_surfidcode = NI_get_attribute(nel, "surface_idcode");
		if (nel_surfidcode == NULL) {
			fprintf(SUMA_STDERR,"Error %s: surface_idcode missing in nel.\nLoose Crosshair\n", FuncName);
			iv3[0] = -1;
		} else {
			iv3[0] = SUMAg_cSV->Focus_SO_ID;
		}
		iv3[1] = -1; /* nothing at the monent for a node based link, from afni */
		
	sprintf(sfield,"iv3");
		sprintf(sdestination,"BindCrossHair");
		if (!SUMA_RegisterEngineData (&EngineData, sfield, (void *)(iv3), sdestination, ssource, NOPE)) {
			fprintf(SUMA_STDERR,"Error %s: Failed to register %s to %s\n", FuncName, sfield, sdestination);
			SUMA_RETURN(NOPE);
		}
		sprintf(CommString,"BindCrossHair~");
		if (!SUMA_Engine (CommString, &EngineData)) {
			fprintf(stderr, "Error %s: SUMA_Engine call failed.\n", FuncName);
		}
		
		/* send cross hair coordinates */
		sprintf(sfield,"fv3");
		sprintf(sdestination,"SetCrossHair");
		sprintf(ssource,"afni");
		if (!SUMA_RegisterEngineData (&EngineData, sfield, (void *)XYZ, sdestination, ssource, NOPE)) {
			fprintf(SUMA_STDERR,"Error %s: Failed to register %s to %s\n", FuncName, sfield, sdestination);
		}
		sprintf(CommString,"Redisplay|SetCrossHair~");
		if (!SUMA_Engine (CommString, &EngineData)) {
			fprintf(stderr, "Error %s: SUMA_Engine call failed.\n", FuncName);
		}		
		
		/* don't free nel, it's freed later on */
		SUMA_RETURN(YUP) ;
	}/* SUMA_crosshair_xyz */
	
	/* SUMA_irgba Node colors */
	if( strcmp(nel->name,"SUMA_irgba") == 0) {/* SUMA_irgba */
		if( nel->vec_len  < 1 || nel->vec_filled <  1) {  /* empty element?             */
			fprintf(SUMA_STDERR,"%s: Empty SUMA_irgba.\n", FuncName);
			Empty_irgba = YUP;
	  	}else {
			if( nel->vec_num != 5 || nel->vec_typ[0] != NI_INT || nel->vec_typ[1] != NI_BYTE || nel->vec_typ[2] != NI_BYTE || nel->vec_typ[3] != NI_BYTE) {
	  			fprintf(SUMA_STDERR,"%s: SUMA_irgba Bad format\n", FuncName);
				SUMA_RETURN(NOPE);
		  }
		}
	  
	  /* show me nel */
	  if (LocalHead) SUMA_nel_stdout (nel);
	  
		/* make sure that the Surface idcode corresponds to the one in focus */
	   nel_surfidcode = NI_get_attribute(nel, "surface_idcode");
		if (nel_surfidcode == NULL) {
			fprintf(SUMA_STDERR,"Error %s: surface_idcode missing in nel.\n", FuncName);
			SUMA_RETURN(NOPE);
		} else {
			if (strcmp(nel_surfidcode, SO->MapRef_idcode_str) != 0) {
				fprintf(SUMA_STDERR,"Error %s: surface_idcode in nel not equal to SO->MapRef_idcode_str.\n", FuncName);
				SUMA_RETURN(NOPE);
			} else {
				if (LocalHead) fprintf(SUMA_STDOUT,"%s: Matching surface idcode\n", FuncName);
			}
		}
	  
	   /* store the node colors */

		#ifdef OLD_FUNC_OVERLAY		
			/*Prep EngineData and fm*/
			EngineData.N_cols = 4;
			EngineData.N_rows = SO->N_Node;
			fm = (float **)SUMA_allocate2D (EngineData.N_rows, EngineData.N_cols, sizeof(float));
			if (fm == NULL) {
				fprintf(stderr,"Error %s: Failed to allocate space for fm\n", FuncName);
				SUMA_RETURN(NOPE);
			}

			/* set all node colors to nothingness */
			for (i=0; i < SO->N_Node; ++i) {
				fm[i][0] = i; /* store the index */
				fm[i][1] = fm[i][2] = fm[i][3] = SUMA_GRAY_NODE_COLOR; /* the grayness of it all */
			}

			inel = (int *)nel->vec[0];
			r = (byte *)nel->vec[1];
			g = (byte *)nel->vec[2];
			b = (byte *)nel->vec[3];

			/* dim colors from maximum intensity to preserve surface shape highlights */
			dimfact = 255.0 / SUMA_DIM_AFNI_COLOR_FACTOR;

			/* set the colored nodes to something */
			for (i=0; i < nel->vec_len; ++i) {
				/*fprintf(SUMA_STDERR,"Node %d: r%d, g%d, b%d\n", inel[i], r[i], g[i], b[i]);*/
				fm[inel[i]][1] = (float)(r[i]) / dimfact ;
				fm[inel[i]][2] = (float)(g[i]) / dimfact ;
				fm[inel[i]][3] = (float)(b[i]) / dimfact ;
			}

			/* Fast method algorithm, requires sorted list 
			ilp = 0;
			nxt = 0;
			while (nxt < nel->vec_len) {
				while (ilp < nel->vec[nxt]) {
					color[ilp][r] = nothing[r];
					color[ilp][g] = nothing[g];
					color[ilp][b] = nothing[b];
					color[ilp][a] = nothing[a];

					++ilp;
				}
				color[ilp][r] = nel->vec[r][ilp];
				color[ilp][g] = nel->vec[g][ilp];
				color[ilp][b] = nel->vec[b][ilp];
				color[ilp][a] = nel->vec[a][ilp];
				++ilp;
				++nxt;
			}
			 Fast method, requires sorted list */

			/*register fm with EngineData */
			sprintf(sfield,"fm");
			sprintf(sdestination,"SetNodeColor");
			sprintf(ssource,"afni");
			if (!SUMA_RegisterEngineData (&EngineData, sfield, (void *)fm, sdestination, ssource, YUP)) {
				fprintf(SUMA_STDERR,"Error %s: Failed to register %s to %s\n", FuncName, sfield, sdestination);
				SUMA_RETURN(NOPE);
			}

			/* call EngineData */
			sprintf(CommString,"Redisplay|SetNodeColor~");
			if (!SUMA_Engine (CommString, &EngineData)) {
				fprintf(stderr, "Error %s: SUMA_Engine call failed.\n", FuncName);
				SUMA_RETURN (NOPE);
			}

			/* free fm since it was registered by pointer and is not automatically freed after the call to SUMA_Engine */
			if (fm) SUMA_free2D ((char **)fm, SO->N_Node);
		#else
			{	
				SUMA_OVERLAYS * tmpptr; 
				int OverInd, _ID;
				SUMA_SurfaceObject *SOmap;
				/* create a color overlay plane */
				/* you could create an overlay plane with partial node coverage but you'd have to clean up and reallocate
				with each new data sent since the number of colored nodes will change. So I'll allocate for the entire node list 
				for the FuncAfni_0 color plane although only some values will be used*/
				
				/* if plane exists use it, else create a new one on the mappable surface */
				if (!SUMA_Fetch_OverlayPointer (SO->Overlays, SO->N_Overlays, "FuncAfni_0", &OverInd)) {
					/* overlay plane not found, create a new one on the mappable surface*/
					if (SUMA_isINHmappable(SO)) {
						SOmap = SO;
					} else {
						_ID = SUMA_findDO(SO->MapRef_idcode_str, SUMAg_DOv, SUMAg_N_DOv);
						if (_ID < 0) {
								/* not found */
								fprintf(SUMA_STDERR, "Error %s: Map reference %s not found.\n",\
								 FuncName, SO->MapRef_idcode_str);
								SUMA_RETURN(NOPE);
							}
						SOmap = (SUMA_SurfaceObject *)(SUMAg_DOv[_ID].OP);
					}
					SOmap->Overlays[SOmap->N_Overlays] = SUMA_CreateOverlayPointer (SOmap->N_Node, "FuncAfni_0");
					if (!SOmap->Overlays[SOmap->N_Overlays]) {
						fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_CreateOverlayPointer.\n", FuncName);
						SUMA_RETURN(NOPE);
					} 
					
					/* make an Inode for the overlay */
					SOmap->Overlays_Inode[SOmap->N_Overlays] = SUMA_CreateInode ((void *)SOmap->Overlays[SOmap->N_Overlays], SOmap->idcode_str);
					if (!SOmap->Overlays_Inode[SOmap->N_Overlays]) {
						fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_CreateInode\n", FuncName);
						SUMA_RETURN(NOPE);
					}
					
					OverInd = SOmap->N_Overlays; 
					SOmap->N_Overlays ++;
					
					/* place the overlay plane on top */
					if(!SUMA_SetPlaneOrder (SOmap->Overlays, SOmap->N_Overlays, "FuncAfni_0", SOmap->N_Overlays-1)) {
						fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_SetPlaneOrder\n", FuncName);
						SUMA_RETURN(NOPE);
					}
					
					/* If the surface was not inherently mappable, create the links to the new overlay plane */
					if (SO != SOmap) {
						/* first turn on some defaults for the mappable surface */
						SOmap->Overlays[OverInd]->Show = YUP;
						SOmap->Overlays[OverInd]->GlobalOpacity = SUMA_AFNI_COLORPLANE_OPACITY;
						SOmap->Overlays[OverInd]->BrightMod = NOPE; 
						
						/* now create the link for SO */
						SO->Overlays_Inode[SO->N_Overlays] = SUMA_CreateInodeLink (SO->Overlays_Inode[SO->N_Overlays],\
										 SOmap->Overlays_Inode[SOmap->N_Overlays-1]);
						if (!SO->Overlays_Inode[SO->N_Overlays]) {
								fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_CreateInodeLink\n", FuncName);
								SUMA_RETURN(NOPE);
							}
						/* now copy the actual overlay plane pointer */
						SO->Overlays[SO->N_Overlays] = SOmap->Overlays[SOmap->N_Overlays-1];
						/*increment the number of overlay planes */
						OverInd = SO->N_Overlays; 
						++SO->N_Overlays;
					}
						 
				}
				
				/* now place dem colors in the overlay plane */
				SO->Overlays[OverInd]->Show = YUP;
				SO->Overlays[OverInd]->GlobalOpacity = SUMA_AFNI_COLORPLANE_OPACITY;
				SO->Overlays[OverInd]->BrightMod = NOPE;
				
				if (!Empty_irgba) {
					inel = (int *)nel->vec[0];
					r = (byte *)nel->vec[1];
					g = (byte *)nel->vec[2];
					b = (byte *)nel->vec[3];

					/* dim colors from maximum intensity to preserve surface shape highlights */
					dimfact = 255.0 / SUMA_DIM_AFNI_COLOR_FACTOR;				/* set the colored nodes to something */
					for (i=0; i < nel->vec_len; ++i) {
						/*fprintf(SUMA_STDERR,"Node %d: r%d, g%d, b%d\n", inel[i], r[i], g[i], b[i]);*/
						SO->Overlays[OverInd]->NodeDef[i] = inel[i];
						SO->Overlays[OverInd]->ColMat[i][0] = (float)(r[i]) / dimfact;
						SO->Overlays[OverInd]->ColMat[i][1] = (float)(g[i]) / dimfact;
						SO->Overlays[OverInd]->ColMat[i][2] = (float)(b[i]) / dimfact;
					}
					SO->Overlays[OverInd]->N_NodeDef = nel->vec_len;
				} else {
					SO->Overlays[OverInd]->N_NodeDef = 0;
				}
				
				/*Do the mix thing */
				/*fprintf(SUMA_STDERR, "%s: Mixing colors ...\n", FuncName);*/
				if (!SUMA_Overlays_2_GLCOLAR4(SO->Overlays, SO->N_Overlays, SO->glar_ColorList, SO->N_Node, \
						SUMAg_cSV->Back_Modfact, SUMAg_cSV->ShowBackground, SUMAg_cSV->ShowForeground)) {
						fprintf (SUMA_STDERR,"Error %s: Failed in SUMA_Overlays_2_GLCOLAR4.\n", FuncName);
						SUMA_RETURN(NOPE);
				}
				
				
				/* file a redisplay request */
				sprintf(CommString,"Redisplay~");
				if (!SUMA_Engine (CommString, NULL)) {
					fprintf(SUMA_STDERR, "Error %s: SUMA_Engine call failed.\n", FuncName);
					SUMA_RETURN(NOPE);
				}
			}
		#endif
		/* don't free nel, it's freed later on */
		SUMA_RETURN(YUP) ;

		 
	}/* SUMA_irgba */

   /*** If here, then name of element didn't match anything ***/

   fprintf(SUMA_STDERR,"Error %s: Unknown NIML input: %s\n", FuncName ,nel->name) ;
   SUMA_RETURN(NOPE) ;
}

/*------------------------------------------------------------------------*/
static int num_workp      = 0 ;
static XtWorkProc * workp = NULL ;
static XtPointer *  datap = NULL ;
static XtWorkProcId wpid ;

/*#define WPDEBUG*/

void SUMA_register_workproc( XtWorkProc func , XtPointer data )
{
   static char FuncName[]={"SUMA_register_workproc"};
	
	if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

	if( func == NULL ){
      fprintf(SUMA_STDERR,"Error %s: func=NULL on entry!\n", FuncName) ;
      SUMA_RETURNe;
   }

   if( num_workp == 0 ){
      workp = (XtWorkProc *) malloc( sizeof(XtWorkProc) ) ;
      datap = (XtPointer *)  malloc( sizeof(XtPointer) ) ;
      wpid  = XtAppAddWorkProc( SUMAg_cSV->X->APP, SUMA_workprocess, NULL ) ;
#ifdef WPDEBUG
      fprintf(stderr,"SUMA_register_workproc: wpid = %x\n",(int)wpid) ;
#endif
   } else {
      workp = (XtWorkProc *) realloc( workp, sizeof(XtWorkProc)*(num_workp+1) ) ;
      datap = (XtPointer*)   realloc( datap, sizeof(XtPointer) *(num_workp+1) ) ;
   }

   workp[num_workp] = func ;
   datap[num_workp] = data ;
   num_workp++ ;

#ifdef WPDEBUG
fprintf(stderr,"SUMA_register_workproc: have %d workprocs\n",num_workp) ;
#endif

   SUMA_RETURNe ;
}

void SUMA_remove_workproc( XtWorkProc func )
{
   int ii , ngood ;
	static char FuncName[]={"SUMA_remove_workproc"};
	
	if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if( func == NULL || num_workp == 0 ){
      fprintf(SUMA_STDERR,"Error %s: *** illegal parameters!\n", FuncName) ;
      SUMA_RETURNe ;
   }

   for( ii=0 ; ii < num_workp ; ii++ ){
      if( func == workp[ii] ) workp[ii] = NULL ;
   }

   for( ii=0,ngood=0 ; ii < num_workp ; ii++ )
      if( workp[ii] != NULL ) ngood++ ;

   if( ngood == 0 ){
#ifdef WPDEBUG
      fprintf(stderr,"SUMA_remove_workproc: No workprocs left\n") ;
#endif
      XtRemoveWorkProc( wpid ) ;
      free(workp) ; workp = NULL ; free(datap) ; datap = NULL ;
      num_workp = 0 ;
   } else {
#ifdef WPDEBUG
      fprintf(stderr,"SUMA_remove_workproc: %d workprocs left\n",ngood) ;
#endif
   }

   SUMA_RETURNe ;
}

Boolean SUMA_workprocess( XtPointer fred )
{
	static char FuncName[]={"SUMA_workprocess"};
   int ii , ngood ;
   Boolean done ;

	if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

#ifdef WPDEBUG
   { static int ncall=0 ;
     if( (ncall++) % 1000 == 0 )
       fprintf(stderr,"SUMA_workprocess: entry %d\n",ncall) ; }
#endif

   if( num_workp == 0 ) SUMA_RETURN(True) ;

   for( ii=0,ngood=0 ; ii < num_workp ; ii++ ){
      if( workp[ii] != NULL ){
         done = workp[ii]( datap[ii] ) ;
         if( done == True ) workp[ii] = NULL ;
         else               ngood++ ;
      }
   }

   if( ngood == 0 ){
#ifdef WPDEBUG
      fprintf(stderr,"Found no workprocs left\n") ;
#endif
      free(workp) ; workp = NULL ; free(datap) ; datap = NULL ;
      num_workp = 0 ;
      SUMA_RETURN(True) ;
   }
   SUMA_RETURN(False) ;
}

/*---------------------------------------------------------------*/

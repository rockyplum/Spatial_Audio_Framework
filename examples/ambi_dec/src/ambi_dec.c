/*
 Copyright 2017-2018 Leo McCormack
 
 Permission to use, copy, modify, and/or distribute this software for any purpose with or
 without fee is hereby granted, provided that the above copyright notice and this permission
 notice appear in all copies.
 
 THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
 SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
 ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
 * Filename:
 *     ambi_dec.c
 * Description:
 *     A frequency-dependent Ambisonic decoder for loudspeakers or headphones. Different
 *     decoder settings can be specified for the low and high frequencies. When utilising
 *     spherical harmonic signals derived from real microphone arrays, this implementation
 *     also allows the decoding order per frequency band to be specified. Optionally, a SOFA
 *     file may be loaded for personalised headphone listening.
 *     The algorithms utilised in this Ambisonic decoder were pieced together and developed
 *     in collaboration with Archontis Politis.
 * Dependencies:
 *     saf_utilities, afSTFTlib, saf_hoa, saf_vbap, saf_hrir, saf_sh
 * Author, date created:
 *     Leo McCormack, 07.12.2017
 */
 
#include "ambi_dec_internal.h"

void ambi_dec_create
(
    void ** const phAmbi
)
{
    ambi_dec_data* pData = (ambi_dec_data*)malloc(sizeof(ambi_dec_data));
    if (pData == NULL) { return;/*error*/ }
    *phAmbi = (void*)pData;
    int i, j, t, ch, band;
    
    /* afSTFT stuff */
    pData->hSTFT = NULL;
    pData->STFTInputFrameTF = (complexVector**)malloc2d(TIME_SLOTS, MAX_NUM_SH_SIGNALS, sizeof(complexVector));
    for(t=0; t<TIME_SLOTS; t++) {
        for(ch=0; ch< MAX_NUM_SH_SIGNALS; ch++) {
            pData->STFTInputFrameTF[t][ch].re = (float*)calloc(HYBRID_BANDS, sizeof(float));
            pData->STFTInputFrameTF[t][ch].im = (float*)calloc(HYBRID_BANDS, sizeof(float));
        }
    }
    pData->STFTOutputFrameTF = NULL;
    pData->tempHopFrameTD = NULL;
    
    /* time-frequency domain frame per order (starting at 1st) */
    for(i=1; i<=SH_ORDER; i++)
        for(band=0; band<HYBRID_BANDS; band++)
            pData->SHframeTF[i-1][band] = malloc( (i+1)*(i+1)*TIME_SLOTS*sizeof(float_complex));
    
    /* codec data */
    pData->pars = (codecPars*)malloc(sizeof(codecPars));
    codecPars* pars = pData->pars;
    for (i=0; i<NUM_DECODERS; i++){
        for(j=0; j<SH_ORDER; j++){
            pars->M_dec[i][j] = NULL;
            pars->M_dec_cmplx[i][j] = NULL;
            pars->M_dec_maxrE[i][j] = NULL;
            pars->M_dec_cmplx_maxrE[i][j] = NULL;
        }
    }
    pars->sofa_filepath = NULL;
    pars->hrirs = NULL;
    pars->hrir_dirs_deg = NULL;
    pars->hrtf_vbap_gtableIdx = NULL;
    pars->hrtf_vbap_gtableComp = NULL;
    pars->itds_s = NULL;
    pars->hrtf_fb = NULL;
    pars->hrtf_fb_mag = NULL;
    
    /* internal parameters */
    pData->new_binauraliseLS = 0;
    pData->binauraliseLS = pData->new_binauraliseLS;
    
    /* flags */
    pData->reInitCodec = 1;
    pData->reInitTFT = 1;
    pData->reInitHRTFs = 1;
    for(ch=0; ch<MAX_NUM_LOUDSPEAKERS; ch++)
        pData->recalc_hrtf_interpFLAG[ch] = 1;
    
    /* default user parameters */
    for(band=0; band<HYBRID_BANDS; band++)
        pData->orderPerBand[band] = SH_ORDER;
    pData->useDefaultHRIRsFLAG = 1; /* pars->sofa_filepath must be valid to set this to 0 */
    ambi_dec_loadPreset(PRESET_T_DESIGN_24, pData->loudpkrs_dirs_deg, &(pData->new_nLoudpkrs), &(pData->loudpkrs_nDims)); 
    pData->nLoudpkrs = pData->new_nLoudpkrs;
    pData->chOrdering = CH_ACN;
    pData->norm = NORM_N3D; 
    pData->dec_method[0] = DECODER_MMD;
    pData->dec_method[1] = DECODER_ALLRAD;
    pData->rE_WEIGHT[0] = 0;
    pData->rE_WEIGHT[1] = 1;
    pData->diffEQmode[0] = AMPLITUDE_PRESERVING;
    pData->diffEQmode[1] = ENERGY_PRESERVING;
    pData->transitionFreq = 1000.0f;
}

void ambi_dec_destroy
(
    void ** const phAmbi
)
{
    ambi_dec_data *pData = (ambi_dec_data*)(*phAmbi);
    codecPars *pars = pData->pars;
    int i, j, t, ch, band;
    
    if (pData != NULL) {
        if(pData->hSTFT!=NULL)
            afSTFTfree(pData->hSTFT);
        for (t = 0; t<TIME_SLOTS; t++) {
            if(pData->STFTInputFrameTF!=NULL){
                for (ch = 0; ch< MAX_NUM_SH_SIGNALS; ch++) {
                    free(pData->STFTInputFrameTF[t][ch].re);
                    free(pData->STFTInputFrameTF[t][ch].im);
                }
            }
            if(pData->binauraliseLS &&  (pData->STFTOutputFrameTF!=NULL) ){
                for (ch = 0; ch< NUM_EARS; ch++) {
                    free(pData->STFTOutputFrameTF[t][ch].re);
                    free(pData->STFTOutputFrameTF[t][ch].im);
                }
            }
            else if((pData->STFTOutputFrameTF!=NULL)){
                for (ch = 0; ch< pData->nLoudpkrs; ch++) {
                    free(pData->STFTOutputFrameTF[t][ch].re);
                    free(pData->STFTOutputFrameTF[t][ch].im);
                }
            }
        }
        free2d((void**)pData->STFTInputFrameTF, TIME_SLOTS);
        free2d((void**)pData->STFTOutputFrameTF, TIME_SLOTS);
        if(pData->binauraliseLS && (pData->tempHopFrameTD!=NULL) )
            free2d((void**)pData->tempHopFrameTD, MAX(NUM_EARS, MAX_NUM_SH_SIGNALS));
        else if(pData->tempHopFrameTD!=NULL)
            free2d((void**)pData->tempHopFrameTD, MAX(pData->nLoudpkrs, MAX_NUM_SH_SIGNALS));
        
        for(i=1; i<=SH_ORDER; i++)
            for(band=0; band<HYBRID_BANDS; band++)
                free(pData->SHframeTF[i-1][band]);

        if(pars->hrtf_vbap_gtableComp!= NULL)
            free(pars->hrtf_vbap_gtableComp);
        if(pars->hrtf_vbap_gtableIdx!= NULL)
            free(pars->hrtf_vbap_gtableIdx);
        if(pars->hrtf_fb!= NULL)
            free(pars->hrtf_fb);
        if(pars->hrtf_fb_mag!= NULL)
            free(pars->hrtf_fb_mag);
        if(pars->itds_s!= NULL)
            free(pars->itds_s);
        if(pars->hrirs!= NULL)
            free(pars->hrirs);
        if(pars->hrir_dirs_deg!= NULL)
            free(pars->hrir_dirs_deg);
        for (i=0; i<NUM_DECODERS; i++){
            for(j=0; j<SH_ORDER; j++){
                free(pars->M_dec[i][j]);
                free(pars->M_dec_cmplx[i][j]);
                free(pars->M_dec_maxrE[i][j]);
                free(pars->M_dec_cmplx_maxrE[i][j]);
            }
        }

        free(pData);
        pData = NULL;
    }
}

void ambi_dec_init
(
    void * const hAmbi,
    int          sampleRate
)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    int band;
    
    /* define frequency vector */
    pData->fs = sampleRate;
    for(band=0; band <HYBRID_BANDS; band++){
        if(sampleRate == 44100)
            pData->freqVector[band] =  (float)__afCenterFreq44100[band];
        else /* Assume 48kHz */
            pData->freqVector[band] =  (float)__afCenterFreq48e3[band];
    } 
}

void ambi_dec_process
(
    void  *  const hAmbi,
    float ** const inputs,
    float ** const outputs,
    int            nInputs,
    int            nOutputs,
    int            nSamples,
    int            isPlaying
)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    codecPars* pars = pData->pars;
    int n, t, sample, ch, ear, i, band, orderBand, nSH_band, decIdx;
    int o[SH_ORDER+2];
    const float_complex calpha = cmplxf(1.0f,0.0f), cbeta = cmplxf(0.0f, 0.0f);
    
#ifdef ENABLE_FADE_IN_OUT
    int applyFadeIn;
    if(pData->reInitTFT || pData->reInitHRTFs)
        applyFadeIn = 1;
    else
        applyFadeIn = 0;
#endif
    /* local copies of user parameters */
    int nLoudspeakers, binauraliseLS;
    int orderPerBand[HYBRID_BANDS], rE_WEIGHT[NUM_DECODERS];
    float transitionFreq;
    DIFFUSE_FIELD_EQ_APPROACH diffEQmode[NUM_DECODERS];
    NORM_TYPES norm;
    
    /* reinitialise if needed */
    if(pData->reInitTFT==1){
        pData->reInitTFT = 2;
        ambi_dec_initTFT(hAmbi); /* always init before codec or hrtfs (will do this better in future release) */
        pData->reInitTFT = 0;
    } /* TODO: do this better for future release */
    if(pData->reInitCodec==1){
        pData->reInitCodec = 2;
        ambi_dec_initCodec(hAmbi);
        pData->reInitCodec = 0;
    }
    if(pData->reInitHRTFs==1){
        pData->reInitHRTFs = 2;
        ambi_dec_initHRTFs(hAmbi);
        pData->reInitHRTFs = 0;
    }
    /* decode audio to loudspeakers or headphones */
    if ( (nSamples == FRAME_SIZE) && (isPlaying) && (pData->reInitCodec==0) && (pData->reInitTFT==0) && (pData->reInitHRTFs==0) ) {
        /* copy user parameters to local variables */
        for(n=0; n<SH_ORDER+2; n++){  o[n] = n*n;  }
        nLoudspeakers = pData->nLoudpkrs;
        memcpy(orderPerBand, pData->orderPerBand, HYBRID_BANDS*sizeof(int));
        transitionFreq = pData->transitionFreq;
        memcpy(diffEQmode, pData->diffEQmode, NUM_DECODERS*sizeof(int));
        binauraliseLS = pData->binauraliseLS;
        norm = pData->norm;
        memcpy(rE_WEIGHT, pData->rE_WEIGHT, NUM_DECODERS*sizeof(int));
        
        /* Load time-domain data */
        for(i=0; i < MIN(MAX_NUM_SH_SIGNALS, nInputs); i++)
            memcpy(pData->SHFrameTD[i], inputs[i], FRAME_SIZE * sizeof(float));
        for(; i<MAX_NUM_SH_SIGNALS; i++)
            memset(pData->SHFrameTD[i], 0, FRAME_SIZE * sizeof(float)); /* fill remaining channels with zeros, to avoid funky behaviour */
#ifdef ENABLE_FADE_IN_OUT
        if(applyFadeIn)
            for(ch=0; ch < MAX_NUM_SH_SIGNALS;ch++)
                for(i=0; i<FRAME_SIZE; i++)
                    pData->SHFrameTD[ch][i] *= (float)i/(float)FRAME_SIZE;
#endif
        
        /* account for input normalisation scheme */
        switch(norm){
            case NORM_N3D:  /* already in N3D, do nothing */
                break;
            case NORM_SN3D: /* convert to N3D */
                for (n = 0; n<SH_ORDER+1; n++)
                    for (ch = o[n]; ch<o[n+1]; ch++)
                        for(i = 0; i<FRAME_SIZE; i++)
                            pData->SHFrameTD[ch][i] *= sqrtf(2.0f*(float)n+1.0f);
                break;
        }
        
        /* Apply time-frequency transform (TFT) */
        for ( t=0; t< TIME_SLOTS; t++) {
            for( ch=0; ch < MAX_NUM_SH_SIGNALS; ch++)
                for ( sample=0; sample < HOP_SIZE; sample++)
                    pData->tempHopFrameTD[ch][sample] = pData->SHFrameTD[ch][sample + t*HOP_SIZE];
            afSTFTforward(pData->hSTFT, (float**)pData->tempHopFrameTD, (complexVector*)pData->STFTInputFrameTF[t]);
        }
        for(n=1; n<=SH_ORDER; n++)
            for(band=0; band<HYBRID_BANDS; band++)
                for( ch=0; ch < (n+1)*(n+1); ch++)
                    for ( t=0; t<TIME_SLOTS; t++)
                        pData->SHframeTF[n-1][band][ch*TIME_SLOTS + t] = cmplxf(pData->STFTInputFrameTF[t][ch].re[band], pData->STFTInputFrameTF[t][ch].im[band]);
        
        /* Decode to loudspeaker set-up */
        memset(pData->outputframeTF, 0, HYBRID_BANDS*MAX_NUM_LOUDSPEAKERS*TIME_SLOTS*sizeof(float_complex));
        for(band=0; band<HYBRID_BANDS; band++){
            orderBand = MAX(MIN(orderPerBand[band], SH_ORDER),1);
            nSH_band = (orderBand+1)*(orderBand+1);
            decIdx = pData->freqVector[band] < transitionFreq ? 0 : 1; /* different decoder for low (0) and high (1) frequencies */
            if(rE_WEIGHT[decIdx]){
                cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nLoudspeakers, TIME_SLOTS, nSH_band, &calpha,
                            pars->M_dec_cmplx_maxrE[decIdx][orderBand-1], nSH_band,
                            pData->SHframeTF[orderBand-1][band], TIME_SLOTS, &cbeta,
                            pData->outputframeTF[band], TIME_SLOTS);
            }
            else{
                cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nLoudspeakers, TIME_SLOTS, nSH_band, &calpha,
                            pars->M_dec_cmplx[decIdx][orderBand-1], nSH_band,
                            pData->SHframeTF[orderBand-1][band], TIME_SLOTS, &cbeta,
                            pData->outputframeTF[band], TIME_SLOTS);
            }
            for(i=0; i<nLoudspeakers; i++){
                for(t=0; t<TIME_SLOTS; t++){
                    if(diffEQmode[decIdx]==AMPLITUDE_PRESERVING)
                        pData->outputframeTF[band][i][t] = crmulf(pData->outputframeTF[band][i][t], pars->M_norm[decIdx][orderBand-1][0]);
                    else
                        pData->outputframeTF[band][i][t] = crmulf(pData->outputframeTF[band][i][t], pars->M_norm[decIdx][orderBand-1][1]);
                }
            }
        }
        
        /* binauralise the loudspeaker signals */
        if(binauraliseLS){
            memset(pData->binframeTF, 0, HYBRID_BANDS*NUM_EARS*TIME_SLOTS * sizeof(float_complex));
            /* interpolate hrtfs and apply to each source */
            for (ch = 0; ch < nLoudspeakers; ch++) {
                if(pData->recalc_hrtf_interpFLAG[ch]){
                    ambi_dec_interpHRTFs(hAmbi, pData->loudpkrs_dirs_deg[ch][0], pData->loudpkrs_dirs_deg[ch][1], pars->hrtf_interp[ch]);
                    pData->recalc_hrtf_interpFLAG[ch] = 0;
                }
                for (band = 0; band < HYBRID_BANDS; band++)
                    for (ear = 0; ear < NUM_EARS; ear++)
                        for (t = 0; t < TIME_SLOTS; t++)
                            pData->binframeTF[band][ear][t] = ccaddf(pData->binframeTF[band][ear][t], ccmulf(pData->outputframeTF[band][ch][t], pars->hrtf_interp[ch][band][ear]));
            }
            
            /* scale by sqrt(number of loudspeakers) */
            for (band = 0; band < HYBRID_BANDS; band++)
                for (ear = 0; ear < NUM_EARS; ear++)
                    for (t = 0; t < TIME_SLOTS; t++)
                        pData->binframeTF[band][ear][t] = crmulf(pData->binframeTF[band][ear][t], 1.0f/sqrtf((float)nLoudspeakers));
        }
        
        /* inverse-TFT */
        for (band = 0; band < HYBRID_BANDS; band++) {
            if(binauraliseLS){
                for (ch = 0; ch < NUM_EARS; ch++) {
                    for (t = 0; t < TIME_SLOTS; t++) {
                        pData->STFTOutputFrameTF[t][ch].re[band] = crealf(pData->binframeTF[band][ch][t]);
                        pData->STFTOutputFrameTF[t][ch].im[band] = cimagf(pData->binframeTF[band][ch][t]);
                    }
                }
            }
            else{
                for (ch = 0; ch < nLoudspeakers; ch++) {
                    for (t = 0; t < TIME_SLOTS; t++) {
                        pData->STFTOutputFrameTF[t][ch].re[band] = crealf(pData->outputframeTF[band][ch][t]);
                        pData->STFTOutputFrameTF[t][ch].im[band] = cimagf(pData->outputframeTF[band][ch][t]);
                    }
                }
            }
        }
        for (t = 0; t < TIME_SLOTS; t++) {
            afSTFTinverse(pData->hSTFT, pData->STFTOutputFrameTF[t], pData->tempHopFrameTD);
            for (ch = 0; ch < MIN(binauraliseLS==1 ? NUM_EARS : nLoudspeakers, nOutputs); ch++)
                for (sample = 0; sample < HOP_SIZE; sample++)
                    outputs[ch][sample + t* HOP_SIZE] = pData->tempHopFrameTD[ch][sample];
            for (; ch < nOutputs; ch++) /* fill remaining channels with zeros */
                for (sample = 0; sample < HOP_SIZE; sample++)
                    outputs[ch][sample + t* HOP_SIZE] = 0.0f;
        }
#ifdef ENABLE_FADE_IN_OUT
        if(pData->reInitTFT || pData->reInitHRTFs)
            for(ch=0; ch < nOutputs; ch++)
                for(i=0; i<FRAME_SIZE; i++)
                    outputs[ch][i] *= (1.0f - (float)(i+1)/(float)FRAME_SIZE);
#endif
    }
    else
        for (ch=0; ch < nOutputs; ch++)
            memset(outputs[ch],0, FRAME_SIZE*sizeof(float));
}


/* Set Functions */

void ambi_dec_refreshSettings(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->reInitCodec = 1;
    pData->reInitTFT = 1;
    pData->reInitHRTFs = 1;
}

void ambi_dec_setDecOrder(void  * const hAmbi, int newValue, int bandIdx)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->orderPerBand[bandIdx] = MIN(MAX(newValue,1), SH_ORDER);
}

void ambi_dec_setDecOrderAllBands(void  * const hAmbi, int newValue)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    int band;
    
    for(band=0; band<HYBRID_BANDS; band++)
        pData->orderPerBand[band] = MIN(MAX(newValue,1), SH_ORDER);
}

void ambi_dec_setLoudspeakerAzi_deg(void* const hAmbi, int index, float newAzi_deg)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    newAzi_deg = MAX(newAzi_deg, -180.0f);
    newAzi_deg = MIN(newAzi_deg, 180.0f);
    pData->loudpkrs_dirs_deg[index][0] = newAzi_deg;
    pData->reInitCodec=1;
    pData->recalc_hrtf_interpFLAG[index] = 1;
}

void ambi_dec_setLoudspeakerElev_deg(void* const hAmbi, int index, float newElev_deg)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    newElev_deg = MAX(newElev_deg, -90.0f);
    newElev_deg = MIN(newElev_deg, 90.0f);
    pData->loudpkrs_dirs_deg[index][1] = newElev_deg;
    pData->reInitCodec=1;
    pData->recalc_hrtf_interpFLAG[index] = 1;
}

void ambi_dec_setNumLoudspeakers(void* const hAmbi, int new_nLoudspeakers)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    int ch;
    float sum_elev;
    pData->new_nLoudpkrs = new_nLoudspeakers > MAX_NUM_LOUDSPEAKERS ? MAX_NUM_LOUDSPEAKERS : new_nLoudspeakers;
    pData->new_nLoudpkrs = MAX(MIN_NUM_LOUDSPEAKERS, pData->new_nLoudpkrs);
    if(pData->nLoudpkrs != pData->new_nLoudpkrs){
        pData->reInitTFT = 1;
        /* check the new dimensionality before reinitialising the codec parameters */
        sum_elev = 0.0f;
        for(ch=0; ch<pData->new_nLoudpkrs; ch++){
            sum_elev += fabsf(pData->loudpkrs_dirs_deg[ch][1]);
        }
        if( (((sum_elev < 5.0f) && (sum_elev > -5.0f))) || (pData->new_nLoudpkrs < 4) )
            pData->loudpkrs_nDims = 2;
        else
            pData->loudpkrs_nDims = 3;
        pData->reInitCodec=1;
        for(ch=0; ch<MAX_NUM_LOUDSPEAKERS; ch++)
            pData->recalc_hrtf_interpFLAG[ch] = 1;
    }
}

void ambi_dec_setBinauraliseLSflag(void* const hAmbi, int newState)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    
    pData->new_binauraliseLS = newState;
    if(pData->new_binauraliseLS != pData->binauraliseLS)
        pData->reInitTFT = 1;
    if(pData->new_binauraliseLS)
        pData->reInitHRTFs = 1;
}

void ambi_dec_setUseDefaultHRIRsflag(void* const hAmbi, int newState)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    
    if((!pData->useDefaultHRIRsFLAG) && (newState)){
        pData->useDefaultHRIRsFLAG = newState;
        pData->reInitHRTFs = 1;
    }
}

void ambi_dec_setSofaFilePath(void* const hAmbi, const char* path)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    codecPars* pars = pData->pars;
    
    pars->sofa_filepath = malloc(strlen(path) + 1);
    strcpy(pars->sofa_filepath, path);
    pData->useDefaultHRIRsFLAG = 0;
    pData->reInitHRTFs = 1;
}

void ambi_dec_setOutputConfigPreset(void* const hAmbi, int newPresetID)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    int ch;
    
    ambi_dec_loadPreset(newPresetID, pData->loudpkrs_dirs_deg, &(pData->new_nLoudpkrs), &(pData->loudpkrs_nDims));
    if(pData->nLoudpkrs != pData->new_nLoudpkrs)
        pData->reInitTFT = 1;
    pData->reInitCodec = 1;
    for(ch=0; ch<MAX_NUM_LOUDSPEAKERS; ch++)
        pData->recalc_hrtf_interpFLAG[ch] = 1;
}

void ambi_dec_setSourcePreset(void* const hAmbi, int newPresetID)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    int band, rangeIdx, curOrder, reverse;
    
    rangeIdx = 0;
    curOrder = 1;
    reverse = 0;
    switch(newPresetID){
        /* Ideal spherical harmonics will have SH_ORDER at all frequencies */
        case MIC_PRESET_IDEAL:
            for(band=0; band<HYBRID_BANDS; band++)
                pData->orderPerBand[band] = SH_ORDER;
            break;
            
        /* For real microphone arrays, the maximum usable spherical harmonic order will depend on frequency  */
#ifdef ENABLE_ZYLIA_MIC_PRESET
        case MIC_PRESET_ZYLIA:
            for(band=0; band<HYBRID_BANDS; band++){
                if(rangeIdx<2*(__Zylia_maxOrder-1)){
                    if(pData->freqVector[band]>__Zylia_freqRange[rangeIdx]){
                        if(!reverse)
                            curOrder++;
                        else
                            curOrder--;
                        reverse = (curOrder == __Zylia_maxOrder) || (reverse) ? 1 : 0;
                        rangeIdx++;
                    }
                }
                pData->orderPerBand[band] = MIN(SH_ORDER,curOrder);
            }
            break;
#endif
            
#ifdef ENABLE_EIGENMIKE32_MIC_PRESET
        case MIC_PRESET_EIGENMIKE32:
            for(band=0; band<HYBRID_BANDS; band++){
                if(rangeIdx<2*(__Eigenmike32_maxOrder-1)){
                    if(pData->freqVector[band]>__Eigenmike32_freqRange[rangeIdx]){
                        if(!reverse)
                            curOrder++;
                        else
                            curOrder--;
                        reverse = (curOrder == __Eigenmike32_maxOrder) || (reverse) ? 1 : 0;
                        rangeIdx++;
                    }
                }
                pData->orderPerBand[band] = MIN(SH_ORDER,curOrder);
            }
            break;
#endif
            
#ifdef ENABLE_DTU_MIC_MIC_PRESET
        case MIC_PRESET_DTU_MIC:
            for(band=0; band<HYBRID_BANDS; band++){
                if(rangeIdx<2*(__DTU_mic_maxOrder-1)){
                    if(pData->freqVector[band]>__DTU_mic_freqRange[rangeIdx]){
                        if(!reverse)
                            curOrder++;
                        else
                            curOrder--;
                        reverse = (curOrder == __DTU_mic_maxOrder) || (reverse) ? 1 : 0;
                        rangeIdx++;
                    }
                }
                pData->orderPerBand[band] = MIN(SH_ORDER,curOrder);
            }
            break;
#endif
    }
}

void ambi_dec_setChOrder(void* const hAmbi, int newOrder)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->chOrdering = (CH_ORDER)newOrder;
}

void ambi_dec_setNormType(void* const hAmbi, int newType)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->norm = (NORM_TYPES)newType;
}

void ambi_dec_setDecMethod(void* const hAmbi, int index, int newID)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->dec_method[index] = newID;
    pData->reInitCodec = 1;
}

void ambi_dec_setDecEnableMaxrE(void* const hAmbi, int index, int newID)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->rE_WEIGHT[index] = newID;
}

void ambi_dec_setDecNormType(void* const hAmbi, int index, int newID)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->diffEQmode[index] = newID;
}

void ambi_dec_setTransitionFreq(void* const hAmbi, float newValue)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    pData->transitionFreq = newValue;
}


/* Get Functions */

int ambi_dec_getDecOrder(void  * const hAmbi, int bandIdx)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->orderPerBand[bandIdx];
}

int ambi_dec_getDecOrderAllBands(void  * const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->orderPerBand[0];
}

void ambi_dec_getDecOrderHandle
(
    void* const hAmbi,
    float** pX_vector,
    int** pY_values,
    int* pNpoints
)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    (*pX_vector) = &pData->freqVector[0];
    (*pY_values) = &pData->orderPerBand[0];
    (*pNpoints) = HYBRID_BANDS;
}

int ambi_dec_getNumberOfBands(void)
{
    return HYBRID_BANDS;
}

float ambi_dec_getLoudspeakerAzi_deg(void* const hAmbi, int index)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->loudpkrs_dirs_deg[index][0];
}

float ambi_dec_getLoudspeakerElev_deg(void* const hAmbi, int index)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->loudpkrs_dirs_deg[index][1];
}

int ambi_dec_getNumLoudspeakers(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->nLoudpkrs;
}

int ambi_dec_getMaxNumLoudspeakers()
{
    return MAX_NUM_LOUDSPEAKERS;
}

int ambi_dec_getBinauraliseLSflag(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->binauraliseLS;
}

int ambi_dec_getUseDefaultHRIRsflag(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->useDefaultHRIRsFLAG;
}

char* ambi_dec_getSofaFilePath(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    codecPars* pars = pData->pars;
    if(pars->sofa_filepath!=NULL)
        return pars->sofa_filepath;
    else
        return "no_file";
}

int ambi_dec_getChOrder(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return (int)pData->chOrdering;
}

int ambi_dec_getNormType(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return (int)pData->norm;
}

int ambi_dec_getDecMethod(void* const hAmbi, int index)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->dec_method[index];
}

int ambi_dec_getDecEnableMaxrE(void* const hAmbi, int index)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->rE_WEIGHT[index];
}

int ambi_dec_getDecNormType(void* const hAmbi, int index)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->diffEQmode[index];
}

float ambi_dec_getTransitionFreq(void* const hAmbi)
{
    ambi_dec_data *pData = (ambi_dec_data*)(hAmbi);
    return pData->transitionFreq;
}




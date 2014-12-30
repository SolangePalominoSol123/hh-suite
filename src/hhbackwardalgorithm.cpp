/*
 * hhbackwardalgorithmscalar.C
 *
 *  Created on: Apr 10, 2014
 *      Author: stefan
 */

#include "hhposteriordecoder.h"

void PosteriorDecoder::backwardAlgorithm(HMM & q, HMM & t, Hit & hit,
		PosteriorMatrix & p_mm, ViterbiMatrix & celloff_matrix, const int elem) {

	// Variable declarations
	int i, j;      // query and template match state indices
	double pmin; // this is the scaled 1 in the SW algorithm that represents a starting alignment
	double scale_prod = scale[q.L + 1];
	int jmin;

	// Initialization of top row, i.e. cells (0,j)
	for (int j = t.L; j >= 1; j--) {
		if (celloff_matrix.getCellOff(q.L,j,elem)){
			p_mm.setPosteriorValue(q.L, j, 0.0);
			m_mm_prev[j] = 0.0;
		}else {
			m_mm_prev[j] = scale[q.L + 1];
			p_mm.setPosteriorValue(q.L, j, p_mm.getPosteriorValue(q.L, j) * scale[q.L + 1] / hit.Pforward);
		}
		m_mi_prev[j] = m_gd_prev[j] = 0.0;
	}

	if (m_local)
		pmin = scale[q.L + 1];
	else
		pmin = 0.0; // transform pmin (for local alignment) to scale of present (i'th) row

	//backward probability calculation
	double final_scale_prod = scale[q.L + 1];

	// Backward algorithm
	// Loop through query positions i
	for (i = q.L - 1; i >= 1; i--) {
		//       if (v>=5) printf("\n");

		if (hit.self)
			jmin = imin(i + SELFEXCL, t.L);
		else
			jmin = 1; // jmin = i+SELFEXCL and not (i+SELFEXCL+1) to set matrix element at boundary to zero

		// Initialize cells at (i,t.L+1)
		scale_prod *= scale[i + 1];
		if (scale_prod < DBL_MIN * 100)
			scale_prod = 0.0;

		if (celloff_matrix.getCellOff(i, t.L, elem)) {
			p_mm.setPosteriorValue(i, t.L, 0.0);
			m_mm_curr[t.L] = 0.0;
		} else {
			m_mm_curr[t.L] = scale_prod;
			p_mm.setPosteriorValue(i, t.L, p_mm.getPosteriorValue(i, t.L) * scale_prod / hit.Pforward);
		}
		pmin *= scale[i + 1]; // transform pmin (for local alignment) to scale of present (i'th) row
		if (pmin < DBL_MIN * 100)
			pmin = 0.0;

		float actual_backward = 0.0;


		m_im_curr[t.L] = m_mi_curr[t.L] = m_dg_curr[t.L] = m_gd_curr[t.L] = 0.0;

		// Loop through template positions j
		for (j = t.L - 1; j >= jmin; j--) {
			// Recursion relations
			//          printf("S[%i][%i]=%4.1f  ",i,j,Score(q->p[i],t->p[j]));

			if (celloff_matrix.getCellOff(i,j,elem))
				m_mm_curr[j] = m_gd_curr[j] = m_im_curr[j] = m_dg_curr[j] =
						m_mi_curr[j] = 0.0;
			else {
				double pmatch = m_mm_prev[j + 1] * p_mm.getColScoreValue(i+1, j+1) * scale[i + 1];

                //TODO!!! save backward profile; More efficient alternative by JS
                actual_backward += pmatch;
                
                m_mm_curr[j] = (pmin         // MM -> EE (End/End, for local alignment)
						+ pmatch * q.tr[i][M2M] * t.tr[j][M2M]           // MM -> MM
						+ m_gd_curr[j + 1] * t.tr[j][M2D]                // MM -> GD (q.tr[i][M2M] is already contained in GD->MM)
						+ m_im_curr[j + 1] * q.tr[i][M2I] * t.tr[j][M2M] // MM -> IM
						+ m_gd_prev[j] * q.tr[i][M2D] * scale[i + 1]     // MM -> DG (t.tr[j][M2M] is already contained in DG->MM)
						+ m_mi_prev[j] * q.tr[i][M2M] * t.tr[j][M2I] * scale[i + 1] // MM -> MI
				);

				m_gd_curr[j] = (pmatch * q.tr[i][M2M] * t.tr[j][D2M]     // GD -> MM
						+ m_gd_curr[j + 1] * t.tr[j][D2D]                // DG -> DG
				);

				m_im_curr[j] = (pmatch * q.tr[i][I2M] * t.tr[j][M2M]     // IM -> MM
						+ m_im_curr[j + 1] * q.tr[i][I2I] * t.tr[j][M2M] // IM -> IM
				);

				m_dg_curr[j] = (pmatch * q.tr[i][D2M] * t.tr[j][M2M]     // DG -> MM
						+ m_gd_prev[j] * q.tr[i][D2D] * scale[i + 1]     // DG -> DG
						//             + B_GD[i][j+1] * q.tr[i][D2M] * t.tr[j][M2D] // DG -> GD
				);

				m_mi_curr[j] = (pmatch * q.tr[i][M2M] * t.tr[j][I2M]     // MI -> MM
						+ m_mi_prev[j] * q.tr[i][M2M] * t.tr[j][I2I] * scale[i + 1] // MI -> MI
						//           + B_IM[i][j+1] * q.tr[i][M2I] * t.tr[j][I2M]   // MI -> IM
				);
			} // end else

			// Calculate posterior probability from Forward and Backward matrix elements
			// old: p_mm.setPosteriorValue(i, j, p_mm.getPosteriorValue(i, j) * m_mm_curr[j] / hit.Pforward);
            p_mm.multiplyPosteriorValue(i, j, m_mm_curr[j] / hit.Pforward);

//            //TODO!!! save backward profile; Markus' version
//            // How is m_backward_profile[q.L] computed in this case?
//            actual_backward += p_mm.getColScoreValue(i, j) * m_mm_curr[j] / hit.Pforward;

        } //end for j

//        // JS: TODO!!! Markus, what does this line do? The variable does not seem to be used
//        actual_backward *= final_scale_prod / scale_prod;
//          m_backward_profile[i] = actual_backward;
   
          //TODO!!! save backward profile; More efficient alternative by JS
          // How is m_backward_profile[1] computed in this case?
          m_backward_profile[i+1] = actual_backward / hit.Pforward;
        
        
		for(int jj = 0; jj <= t.L; jj++) {
			m_mm_prev[jj] = m_mm_curr[jj];
			m_gd_prev[jj] = m_dg_curr[jj];
			m_mi_prev[jj] = m_mi_curr[jj];
		}
	} // end for i

	/*
       // Debugging output
       if (v>=6)
       {
       const int i0=0, i1=q.L;
       const int j0=0, j1=t.L;
       double scale_prod[q.L+2];
       scale_prod[q.L] = scale[q.L+1];
       for (i=q.L-1; i>=1; i--) scale_prod[i] = scale_prod[i+1] * scale[i+1];

       printf("\nBwd      scale     ");
       for (j=j0; j<=j1; ++j) printf("%3i     ",j);
       printf("\n");
       for (i=i0; i<=i1; ++i)
       {
       printf("%3i: %9.3G ",i,1/scale_prod[i]);
       for (j=j0; j<=j1; ++j)
       printf("%7.4f ",(B_MM[i][j]+B_MI[i][j]+B_IM[i][j]+B_DG[i][j]+B_GD[i][j]) * p_mm.getColScoreValue(i, j) * fpow2(ScoreSS(q,t,i,j)) ));
       printf("\n");

       //      printf("MM   %9.5f ",1/scale[i]);
       //      for (j=j0; j<=j1; ++j)
       //        printf("%7.4f ",B_MM[i][j] * p_mm.getColScoreValue(i, j) *fpow2(ScoreSS(q,t,i,j)) ));
       //      printf("\n");
       }
       printf("\nPost     scale     ");
       for (j=j0; j<=j1; ++j) printf("%3i     ",j);
       printf("\n");
       for (i=i0; i<=i1; ++i)
       {
       printf("%3i: %9.3G ",i,1/scale_prod[i]);
       for (j=j0; j<=j1; ++j)
       printf("%7.4f ",B_MM[i][j]*P_MM[i][j]/Pforward);
       printf("\n");
       }
       printf("\n");
       }

       if (v>=4) printf("\nForward total probability ratio: %8.3G\n",Pforward);
       */

	return;

}



template <int dim>
void crystalPlasticity<dim>::calculatePlasticity(unsigned int cellID,
                                                 unsigned int quadPtID)
{
    
    F_tau=F; // Deformation Gradient
    FullMatrix<double> FE_t(dim,dim),FP_t(dim,dim);  //Elastic and Plastic deformation gradient
    Vector<double> s_alpha_t(n_slip_systems); // Slip resistance
    Vector<double> rot1(dim);// Crystal orientation (Rodrigues representation)
    
    int old_precision = std::cout.precision();
    
    // Tolerance
    
    double tol1=modelStressTolerance;
    
    
    
    std::cout.precision(16);
    
    FE_t=Fe_conv[cellID][quadPtID];
    FP_t=Fp_conv[cellID][quadPtID];
    s_alpha_t=s_alpha_conv[cellID][quadPtID];
    rot1=rot[cellID][quadPtID];
    
    
    
    // Rotation matrix of the crystal orientation
    FullMatrix<double> rotmat(dim,dim);
    rotmat=0.0;
    odfpoint(rotmat,rot1);
    
    
    FullMatrix<double> temp(dim,dim),temp1(dim,dim),temp2(dim,dim),temp3(dim,dim),temp4(dim,dim),temp5(dim,dim),temp6(dim,dim); // Temporary matrices
    FullMatrix<double> T_tau(dim,dim),P_tau(dim,dim);
    FullMatrix<double> Fpn_inv(dim,dim),FE_tau_trial(dim,dim),F_trial(dim,dim),CE_tau_trial(dim,dim),FP_t2(dim,dim),Ee_tau_trial(dim,dim);
    
    
    
    //convert to crystal coordinates F_tau=R'*F_tau*R
    temp=0.0;
    rotmat.Tmmult(temp,F_tau);
    temp.mmult(F_tau,rotmat);
    
    
    // Calculation of Schmid Tensors  and B= symm(FE_tau_trial'*FE_tau_trial*S_alpha)
    FullMatrix<double> SCHMID_TENSOR1(n_slip_systems*dim,dim),B(n_slip_systems*dim,dim);
    Vector<double> m1(dim),n1(dim);
    
    for(unsigned int i=0;i<n_slip_systems;i++){
        for (unsigned int j=0;j<dim;j++){
            m1(j)=m_alpha[i][j];
            n1(j)=n_alpha[i][j];
        }
        
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                temp[j][k]=m1(j)*n1(k);
                SCHMID_TENSOR1[dim*i+j][k]=m1(j)*n1(k);
            }
        }
        
        CE_tau_trial.mmult(temp2,temp);
        temp2.symmetrize();
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                B[dim*i+j][k]=2*temp2[j][k];
            }
        }
    }
    
    // Elastic Modulus
    
    FullMatrix<double> Dmat2(2*dim,2*dim),TM(dim*dim,dim*dim);
    Vector<double> vec1(2*dim),vec2(dim*dim);

    
    for(unsigned int i=0;i<6;i++){
        for(unsigned int j=0;j<6;j++){
            Dmat2[i][j] = elasticStiffness[i][j];
        }
    }

    
    vec1(0)=0;vec1(1)=5;vec1(2)=4;vec1(3)=1;vec1(4)=3;vec1(5)=2;
    vec2(0)=0;vec2(1)=5;vec2(2)=4;vec2(3)=5;vec2(4)=1;vec2(5)=3;vec2(6)=4;vec2(7)=3;vec2(8)=2;
    
    
    
    for(unsigned int i=0;i<9;i++){
        for(unsigned int j=0;j<9;j++){
            TM[i][j]=Dmat2(vec2(i),vec2(j));
        }
    }
    
    Vector<double> s_alpha_tau;
    FP_tau=FP_t;
    FE_tau.reinit(dim,dim);
    Fpn_inv=0.0; Fpn_inv.invert(FP_t);
    F_tau.mmult(FE_tau,Fpn_inv);
    s_alpha_tau=s_alpha_t;
    
    Vector<double> s_beta(n_slip_systems),h_beta(n_slip_systems),delh_beta_dels(n_slip_systems),h0(n_slip_systems),a_pow(n_slip_systems),s_s(n_slip_systems);
    FullMatrix<double> h_alpha_beta_t(n_slip_systems,n_slip_systems),A(n_slip_systems,n_slip_systems);
    FullMatrix<double> del_FP(dim,dim);
    FullMatrix<double> A_PA;
    Vector<double> active;
    Vector<double> PA, PA_temp(1);
    Vector<double> resolved_shear_tau_trial(n_slip_systems),b(n_slip_systems),resolved_shear_tau(n_slip_systems);
    Vector<double> x_beta_old(n_slip_systems);
    
    Vector<double> x_beta(n_slip_systems);
    
    FullMatrix<double> PK1_Stiff(dim*dim,dim*dim);
    
    FullMatrix<double> delFp_delF(dim*dim,dim*dim),delFp_delF2(dim*dim,dim*dim),delFp_delF_prev(dim*dim,dim*dim),dels_delF(n_slip_systems,dim*dim),dels_delF_prev(n_slip_systems,dim*dim),A2;
    FullMatrix<double> delFe_delF(dim*dim,dim*dim),delEtrial_delF(dim*dim,dim*dim),deltau_delF(dim*dim,dim*dim),delT_delF(dim*dim,dim*dim),delb_delF,delgamma_delF,S_PA,A_ds(n_slip_systems,n_slip_systems),delgamma_delF2(n_slip_systems,dim*dim),delTstar_delF(dim*dim,dim*dim);
    FullMatrix<double> Ce_tau(dim,dim),T_star_tau(dim,dim);
    FullMatrix<double> T_star_tau_trial(dim,dim),diff_FP(dim,dim);
    
    
    
    delFp_delF=0.0;
    dels_delF=0.0;
    
    double det_FE_tau,det_F_tau, det_FP_tau;
    int n_PA=0;	// Number of active slip systems
    
    
    
    int iter1=1;
    int flag2=0;
    
    while (iter1) {
        
        if(iter1>modelMaxSlipSearchIterations){
            flag2=1;
            break;
        }
        
        FP_t2=FP_tau;
        Fpn_inv=0.0; Fpn_inv.invert(FP_t2);
        FE_tau_trial=0.0;
        F_trial=0.0;
        F_tau.mmult(FE_tau_trial,Fpn_inv);F_trial = FE_tau_trial;
        
        temp.reinit(dim,dim); temp=0.0;
        temp=FE_tau_trial;
        FE_tau_trial.Tmmult(CE_tau_trial,temp);
        Ee_tau_trial=CE_tau_trial;
        temp=IdentityMatrix(dim);
        for(unsigned int i=0;i<dim;i++){
            for(unsigned int j=0;j<dim;j++){
                Ee_tau_trial[i][j] = 0.5*(Ee_tau_trial[i][j]-temp[i][j]);
            }
        }
        
        
        
        
        //% % % % % STEP 2 % % % % %
        // Calculate the trial stress T_star_tau_trial
        Vector<double> tempv1(6),tempv2(6);
        tempv1=0.0;
        Dmat.vmult(tempv1, vecform(Ee_tau_trial));
        matform(T_star_tau_trial,tempv1);
        T_star_tau.equ(1.0,T_star_tau_trial);
        
        det_FE_tau=FE_tau.determinant();
        temp.reinit(dim,dim); FE_tau.mmult(temp,T_star_tau_trial);
        temp.equ(1.0/det_FE_tau,temp); temp.mTmult(T_tau,FE_tau);
        det_F_tau=F_tau.determinant();
        temp2.reinit(dim,dim);
        temp.invert(F_tau); T_tau.mTmult(temp2,temp);
        
        
        P_tau.equ(det_FE_tau,temp2);
        
        
        //% % % % % STEP 3 % % % % %
        // Calculate the trial resolved shear stress resolved_shear_tau_trial for each slip system
        
        resolved_shear_tau_trial=0.0;
        
        CE_tau_trial.mmult(temp,T_star_tau_trial);
        
        n_PA=0;	// Number of active slip systems
        
        resolved_shear_tau_trial=0.0;
        
        for(unsigned int i=0;i<n_slip_systems;i++){
            
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    resolved_shear_tau_trial(i)+=temp[j][k]*SCHMID_TENSOR1[dim*i+j][k];
                }
            }
            
            if(i>numSlipSystems-1){
                if(resolved_shear_tau_trial(i)<0)
                    resolved_shear_tau_trial(i)=0;
            }
            
            //% % % % % STEP 4 % % % % %
            //Determine the set set of the n potentially active slip systems
            b(i)=fabs(resolved_shear_tau_trial(i))-s_alpha_tau(i);
            if( b(i)>=tol1){
                if(n_PA==0){
                    n_PA=n_PA+1;
                    PA.reinit(n_PA);
                    PA(0)=i;
                }
                else{
                    PA_temp=PA;
                    n_PA=n_PA+1;
                    PA.reinit(n_PA);
                    for (unsigned int j=0;j<(n_PA-1);j++){
                        PA(j)=PA_temp(j);
                    }
                    PA(n_PA-1)=i;
                    PA_temp.reinit(n_PA);     //%%%%% Potentially active slip systems
                }
            }
            //resolved_shear_tau(i)=(resolved_shear_tau_trial(i));
        }
        
        
        if(n_PA==0)
            break;
        
        
        //% % % % % STEP 5 % % % % %
        //Calculate the shear increments from the consistency condition
        s_beta=s_alpha_tau;

        // Single slip hardening rate
        for(unsigned int i=0;i<numSlipSystems;i++){
            h_beta(i)=initialHardeningModulus[i]*pow((1-s_beta(i)/saturationStress[i]),powerLawExponent[i]);
        }
        
        for(unsigned int i=0;i<numTwinSystems;i++){
            h_beta(numSlipSystems+i)=initialHardeningModulusTwin[i]*pow((1-s_beta(numSlipSystems+i)/saturationStressTwin[i]),powerLawExponentTwin[i]);
        }

        
        for(unsigned int i=0;i<n_slip_systems;i++){
            for(unsigned int j=0;j<n_slip_systems;j++){
                h_alpha_beta_t[i][j] = q[i][j]*h_beta(j);
                A[i][j]=h_alpha_beta_t[i][j];
            }
        }
        
        // Calculate the Stiffness Matrix A
        for(unsigned int i=0;i<n_slip_systems;i++){
            for(unsigned int j=0;j<n_slip_systems;j++){
                temp1.reinit(dim,dim); temp1=0.0;
                
                for(unsigned int k=0;k<dim;k++){
                    for(unsigned int l=0;l<dim;l++){
                        temp[k][l]=SCHMID_TENSOR1(dim*j+k,l);
                    }
                }
                temp2.reinit(dim,dim); CE_tau_trial.mmult(temp2,temp);
                temp2.symmetrize();
                tempv1=0.0; Dmat.vmult(tempv1, vecform(temp2));
                temp3=0.0; matform(temp3,tempv1);
                
                CE_tau_trial.mmult(temp1,temp3);
                temp3=0.0; temp2.mmult(temp3,T_star_tau_trial);
                
                temp1.add(2.0,temp3);
                
                for(unsigned int k=0;k<dim;k++){
                    for(unsigned int l=0;l<dim;l++){
                        if((resolved_shear_tau_trial(i)<0.0)^(resolved_shear_tau_trial(j)<0.0))
                            A[i][j]-=SCHMID_TENSOR1(dim*i+k,l)*temp1[k][l];
                        else
                            A[i][j]+=SCHMID_TENSOR1(dim*i+k,l)*temp1[k][l];
                        
                    }
                }
            }
        }
        
        
        
        
        x_beta_old=0.0;
        
        int count1=0;
        
        Vector<double> b_PA(n_PA);
        
        for(unsigned int i=0;i<n_PA;i++){
            b_PA(i)=b(PA(i));
        }
        
        
        
        
        //bool x1=1;
        
        while((b_PA.linfty_norm())>tol1) {
            
            count1=count1+1;
            
            
            if(count1>modelMaxSolverIterations)
                break;
            
            x_beta=0.0;
            
            
            
            //Modified slip system search for adding corrective term
            // [x_beta] = INACTIVE_SLIP_REMOVAL(A,b,PA,x_beta_old);
            inactive_slip_removal(active,x_beta_old,x_beta,n_PA,PA,b,A,A_PA);
            temp.reinit(dim,dim);
            del_FP.reinit(dim,dim);
            del_FP=0.0;
            for (unsigned int i=0;i<n_slip_systems;i++){
                for (unsigned int j=0;j<dim;j++){
                    for (unsigned int k=0;k<dim;k++){
                        temp[j][k]=SCHMID_TENSOR1[dim*i+j][k];
                    }
                }
                
                temp.mmult(temp2,FP_t);
                for (unsigned int j=0;j<dim;j++){
                    for (unsigned int k=0;k<dim;k++){
                        if(resolved_shear_tau_trial(i)>0)
                            del_FP[j][k]=del_FP[j][k]+x_beta_old(i)*temp2[j][k];
                        else
                            del_FP[j][k]=del_FP[j][k]-x_beta_old(i)*temp2[j][k];
                    }
                }
                
            }
            
            matrixExponential(del_FP).mmult(FP_tau,FP_t2);
            
            // % % % % % STEP 8 % % % % %
            temp.invert(FP_tau);
            F_tau.mmult(FE_tau,temp);
            
            FE_tau.Tmmult(Ce_tau,FE_tau);
            
            Ee_tau_trial=Ce_tau;
            temp=IdentityMatrix(dim);
            for(unsigned int i=0;i<dim;i++){
                for(unsigned int j=0;j<dim;j++){
                    Ee_tau_trial[i][j] = 0.5*(Ee_tau_trial[i][j]-temp[i][j]);
                }
            }
            
            
            Dmat.vmult(tempv1, vecform(Ee_tau_trial));
            matform(T_star_tau,tempv1);
            
            
            resolved_shear_tau=0.0;
            
            Ce_tau.mmult(temp,T_star_tau);
            
            for(unsigned int i=0;i<n_slip_systems;i++){
                
                for (unsigned int j=0;j<dim;j++){
                    for (unsigned int k=0;k<dim;k++){
                        resolved_shear_tau(i)+=temp[j][k]*SCHMID_TENSOR1[dim*i+j][k];
                    }
                }
            }
            
            
            // % % % % % STEP 9 % % % % %
            
            temp.reinit(dim,dim);
            det_FE_tau=FE_tau.determinant();
            FE_tau.mmult(temp,T_star_tau); temp.equ(1.0/det_FE_tau,temp);
            temp.mTmult(T_tau,FE_tau);
            
            det_F_tau=F_tau.determinant();
            temp.invert(F_tau); T_tau.mTmult(P_tau,temp);
            P_tau.equ(det_F_tau,P_tau);
            
            
            
            double h1=0;
            for(unsigned int i=0;i<n_slip_systems;i++){
                h1=0;
                for(unsigned int j=0;j<n_slip_systems;j++){
                    h1=h1+h_alpha_beta_t(i,j)*x_beta(j);
                }
                s_alpha_tau(i)=s_alpha_tau(i)+h1;
            }
            
            
            for (unsigned int i=0;i<numSlipSystems;i++){
            
                    if(s_alpha_tau(i)>saturationStress[i])
                        s_alpha_tau(i)=saturationStress[i];
            
            }
            
            for (unsigned int i=0;i<numTwinSystems;i++){
                
                if(s_alpha_tau(numSlipSystems+i)>saturationStressTwin[i])
                    s_alpha_tau(numSlipSystems+i)=saturationStressTwin[i];
                
            }
            
            
            
            for(unsigned int i=0;i<n_slip_systems;i++){
                if((resolved_shear_tau_trial(i)>0.0))
                    b(i)=resolved_shear_tau(i)-s_alpha_tau(i);
                else
                    b(i)=-resolved_shear_tau(i)-s_alpha_tau(i);
            }
            
            b_PA.reinit(n_PA);
            
            for(unsigned int i=0;i<n_PA;i++){
                b_PA(i)=b(PA(i));
            }
            bool x1=(b_PA.linfty_norm())>tol1;
            
            
        }
        
        
        for (unsigned int i=0;i<numTwinSystems;i++){
            twinfraction_iter[cellID][quadPtID][i]=twinfraction_conv[cellID][quadPtID][i]+x_beta_old[i+numSlipSystems]/twinShear;
        }
        
        for (unsigned int i=0;i<numSlipSystems;i++){
            slipfraction_iter[cellID][quadPtID][i]=slipfraction_conv[cellID][quadPtID][i]+x_beta_old[i]/twinShear;
        }
        
        
        
        Fpn_inv=0.0; Fpn_inv.invert(FP_tau);
        delFp_delF_prev=delFp_delF;
        dels_delF_prev=dels_delF;
        
        
        delFe_delF=0.0;
        temp1.reinit(dim,dim);
        F_tau.mmult(temp1,Fpn_inv);
        
        for (unsigned int i=0;i<dim;i++){
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    for (unsigned int l=0;l<dim;l++){
                        for (unsigned int a=0;a<dim;a++){
                            for (unsigned int b=0;b<dim;b++){
                                delFe_delF(3*i+j,3*k+l)=delFe_delF(3*i+j,3*k+l)-temp1(i,a)*delFp_delF(3*a+b,3*k+l)*Fpn_inv(b,j);
                            }
                        }
                        if(i==k){
                            delFe_delF(3*i+j,3*k+l)=delFe_delF(3*i+j,3*k+l)+Fpn_inv(l,j);
                        }
                    }
                }
            }
        }
        
        delEtrial_delF=0.0;
        for (unsigned int i=0;i<dim;i++){
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    for (unsigned int l=0;l<dim;l++){
                        for (unsigned int a=0;a<dim;a++){
                            
                            
                            delEtrial_delF(3*(i)+j,3*(k)+l)=delEtrial_delF(3*(i)+j,3*(k)+l)+0.5*(delFe_delF(3*(a)+i,3*(k)+l)*FE_tau(a,j)+delFe_delF(3*(a)+j,3*(k)+l)*FE_tau(a,i));
                        }
                    }
                }
            }
        }
        
        
        
        deltau_delF=0.0;
        temp.reinit(dim,dim);
        temp=0.0;
        temp.Tadd(-1.0,del_FP);
        temp1.reinit(dim,dim);
        temp1=matrixExponential(temp);
        
        
        temp1.reinit(dim,dim);
        temp=0.0;
        temp.add(-1.0,del_FP);
        temp2.reinit(dim,dim);
        temp2=matrixExponential(temp);
        TM.mmult(delT_delF,delEtrial_delF);
        
        for (unsigned int i=0;i<dim;i++){
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    for (unsigned int l=0;l<dim;l++){
                        for (unsigned int a=0;a<dim;a++){
                            
                            deltau_delF(3*(i)+j,3*(k)+l)=deltau_delF(3*(i)+j,3*(k)+l)+ 2* delEtrial_delF(3*(i)+a,3*(k)+l)*T_star_tau(a,j)+Ce_tau(i,a)*delT_delF(3*(a)+j,3*(k)+l);
                            
                        }
                    }
                }
            }
        }
        
        
        dels_delF=0.0;
        
        delh_beta_dels=0.0;
        
        // Hardening modulus
        for(unsigned int i=0;i<numSlipSystems;i++){
            delh_beta_dels(i)=initialHardeningModulus[i]*pow((1-s_alpha_tau(i)/saturationStress[i]),(powerLawExponent[i]-1))*(-1.0/saturationStress[i]);
        }

        
        for(unsigned int i=0;i<numTwinSystems;i++){
           delh_beta_dels(i+numSlipSystems)=initialHardeningModulusTwin[i]*pow((1-s_alpha_tau(i+numSlipSystems)/saturationStressTwin[i]),(powerLawExponentTwin[i]-1))*(-1.0/saturationStressTwin[i]);
        }


        
        FullMatrix<double> term_ds(n_slip_systems,n_slip_systems);
        term_ds=0.0;
        
        for(unsigned int k=0;k<n_slip_systems;k++){
            for(unsigned int l=0;l<n_slip_systems;l++){
                
                term_ds(k,l)=x_beta_old(l)*q(k,l)*delh_beta_dels(l);
            }
        }
        
        
        temp.reinit(n_slip_systems,n_slip_systems);
        temp=IdentityMatrix(n_slip_systems);
        temp.add(-1.0,term_ds);
        
        temp1.reinit(n_slip_systems,n_slip_systems);
        temp1.invert(temp);
        temp1.mmult(dels_delF,dels_delF_prev);
        
        delb_delF.reinit(n_PA,dim*dim);
        delb_delF=0.0;
        
        for(unsigned int k=0;k<n_PA;k++){
            tempv1.reinit(dim*dim);
            int itr=0;
            for(unsigned int i=0;i<dim;i++){
                for(unsigned int j=0;j<dim;j++){
                    tempv1(itr)=SCHMID_TENSOR1(dim*PA(k)+i,j);
                    itr=itr+1;
                }
            }
            
            tempv2.reinit(dim*dim);
            deltau_delF.Tvmult(tempv2,tempv1);
            if(resolved_shear_tau_trial(PA(k))<0){
                tempv2.equ(-1.0,tempv2);
            }
            
            for(unsigned int l=0;l<(dim*dim);l++){
                
                delb_delF(k,l)=tempv2(l);
            }
        }
        
        
        double tol2=1.0;
        int count3=0;
        temp1.reinit(dim,dim);
        temp2.reinit(dim,dim);
        temp1=0.0;
        temp2=IdentityMatrix(dim);
        while(tol2>max(tol1/1e4,1e-12)){
            count3=count3+1;
            del_FP.mmult(temp1,temp2);
            temp1.equ((1.0/count3),temp1);
            tol2=temp1.frobenius_norm();
            temp2=temp1;
        }
        
        
        A2.reinit(n_PA,n_PA);
        A_ds=h_alpha_beta_t;
        
        for(unsigned int i=0;i<n_PA;i++){
            for(unsigned int j=0;j<n_PA;j++){
                A2(i,j)=h_alpha_beta_t(PA(i),PA(j));
            }
        }
        
        
        //Calculate the Stiffness Matrix A
        for(unsigned int j=0;j<n_PA;j++){
            temp.reinit(dim,dim); temp=0.0;
            
            for(unsigned int k=0;k<dim;k++){
                for(unsigned int l=0;l<dim;l++){
                    temp[k][l]=SCHMID_TENSOR1(dim*PA(j)+k,l);
                }
            }
            
            diff_FP.reinit(dim,dim);
            diff_FP=temp;
            
            for(unsigned int k=1;k<=count3;k++){
                
                temp4=0.0;
                
                for(unsigned int l=0;l<=k;l++){
                    
                    temp1.reinit(dim,dim);
                    temp2.reinit(dim,dim);
                    temp3.reinit(dim,dim);
                    temp1=IdentityMatrix(dim);
                    temp2=IdentityMatrix(dim);
                    
                    for (unsigned int m=0;m<l;m++){
                        temp3=temp1;
                        temp3.mmult(temp1,del_FP);
                    }
                    
                    for (unsigned int m=0;m<(k-l);m++){
                        temp3=temp2;
                        temp3.mmult(temp2,del_FP);
                    }
                    
                    temp5=0.0;
                    temp6=0.0;
                    temp1.mmult(temp5,temp);
                    temp5.mmult(temp6,temp2);
                    temp4.add(1.0,temp6);
                    
                }
                temp4.equ(pow(-1.0,k)/tgamma(k+2),temp4);
                diff_FP.add(1.0,temp4);
            }
            
            for(unsigned int i=0;i<n_PA;i++){
                
                temp5=del_FP;
                temp5.equ(-1.0,temp5);
                temp6.reinit(dim,dim); CE_tau_trial.mmult(temp6,matrixExponential(temp5));
                diff_FP.Tmmult(temp2,temp6);
                temp2.symmetrize();
                tempv1.reinit(2*dim);
                tempv1=0.0; Dmat.vmult(tempv1, vecform(temp2));
                temp3=0.0; matform(temp3,tempv1);
                
                Ce_tau.mmult(temp,temp3);
                temp3=0.0; temp2.mmult(temp3,T_star_tau);
                
                temp.add(2.0,temp3);
                
                
                for(unsigned int k=0;k<dim;k++){
                    for(unsigned int l=0;l<dim;l++){
                        if((resolved_shear_tau_trial(PA(i))<0.0)^(resolved_shear_tau_trial(PA(j))<0.0))
                            A2[i][j]-=SCHMID_TENSOR1(dim*PA(i)+k,l)*temp[k][l];
                        else
                            A2[i][j]+=SCHMID_TENSOR1(dim*PA(i)+k,l)*temp[k][l];
                        
                    }
                }
            }
        }
        
        
        temp1.reinit(n_PA,n_PA);
        temp1.invert(A2);
        temp2.reinit(n_PA,dim*dim);
        
        for(unsigned int i=0;i<n_PA;i++){
            for(unsigned int j=0;j<dim*dim;j++){
                temp2[i][j]=delb_delF[i][j]-dels_delF[PA(i)][j];
            }
        }
        delgamma_delF.reinit(n_PA,dim*dim);
        temp1.mmult(delgamma_delF,temp2);
        
        delgamma_delF2=0.0;
        for(unsigned int i=0;i<n_PA;i++){
            for(unsigned int j=0;j<dim*dim;j++){
                delgamma_delF2[PA(i)][j]=delgamma_delF[i][j];
            }
        }
        
        
        
        
        S_PA.reinit(dim*dim,n_PA);
        
        for(unsigned int j=0;j<n_PA;j++){
            temp.reinit(dim,dim); temp=0.0;
            
            for(unsigned int k=0;k<dim;k++){
                for(unsigned int l=0;l<dim;l++){
                    temp[k][l]=SCHMID_TENSOR1(dim*PA(j)+k,l);
                }
            }
            
            diff_FP.reinit(dim,dim);
            diff_FP=temp;
            
            for(unsigned int k=1;k<=count3;k++){
                
                temp4=0.0;
                
                for(unsigned int l=0;l<=k;l++){
                    
                    temp1.reinit(dim,dim);
                    temp2.reinit(dim,dim);
                    temp3.reinit(dim,dim);
                    temp1=IdentityMatrix(dim);
                    temp2=IdentityMatrix(dim);
                    
                    for (unsigned int m=0;m<l;m++){
                        temp3=temp1;
                        temp3.mmult(temp1,del_FP);
                    }
                    
                    for (unsigned int m=0;m<(k-l);m++){
                        temp3=temp2;
                        temp3.mmult(temp2,del_FP);
                    }
                    
                    temp5=0.0;
                    temp6=0.0;
                    temp1.mmult(temp5,temp);
                    temp5.mmult(temp6,temp2);
                    temp4.add(1.0,temp6);
                    
                }
                temp4.equ(1/tgamma(k+2),temp4);
                diff_FP.add(1.0,temp4);
            }
            
            temp1.reinit(dim,dim);
            diff_FP.mmult(temp1,FP_t2);
            //tempv1.reinit(dim*dim);
            
            for(unsigned int k=0;k<dim;k++){
                for(unsigned int l=0;l<dim;l++){
                    S_PA(3*k+l,j)=temp1(k,l);
                    if(resolved_shear_tau_trial(PA(j))<0)
                        S_PA(3*k+l,j)=-temp1(k,l);
                    
                }
            }
            
            
            
        }
        
        
        S_PA.mmult(delFp_delF2,delgamma_delF);
        
        delFp_delF=0.0;
        temp1.reinit(dim,dim);
        temp1=matrixExponential(del_FP);
        
        for (unsigned int i=0;i<dim;i++){
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    for (unsigned int l=0;l<dim;l++){
                        for (unsigned int a=0;a<dim;a++){
                            
                            delFp_delF(3*(i)+j,3*(k)+l)=delFp_delF(3*(i)+j,3*(k)+l)+temp1(i,a)*delFp_delF_prev(3*(a)+j,3*(k)+l);
                            
                            
                        }
                    }
                }
            }
        }
        
        
        delFp_delF.add(1.0,delFp_delF2);
        
        temp1.reinit(n_slip_systems,dim*dim);
        A_ds.mmult(temp1,delgamma_delF2);
        
        dels_delF_prev=dels_delF;
        dels_delF_prev.add(1.0,temp1);
        
        iter1=iter1+1;
        
    }
    
    
    
    delFe_delF=0.0;
    temp1.reinit(dim,dim);
    F_tau.mmult(temp1,Fpn_inv);
    
    for (unsigned int i=0;i<dim;i++){
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                for (unsigned int l=0;l<dim;l++){
                    for (unsigned int a=0;a<dim;a++){
                        for (unsigned int b=0;b<dim;b++){
                            delFe_delF(3*i+j,3*k+l)=delFe_delF(3*i+j,3*k+l)-temp1(i,a)*delFp_delF(3*a+b,3*k+l)*Fpn_inv(b,j);
                        }
                    }
                    if(i==k){
                        delFe_delF(3*i+j,3*k+l)=delFe_delF(3*i+j,3*k+l)+Fpn_inv(l,j);
                    }
                }
            }
        }
    }
    
    delTstar_delF=0.0;
    
    
    
    for (unsigned int i=0;i<dim;i++){
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                for (unsigned int l=0;l<dim;l++){
                    for (unsigned int a=0;a<dim;a++){
                        for (unsigned int b=0;b<dim;b++){
                            for (unsigned int c=0;c<dim;c++){
                                
                                
                                delTstar_delF(3*(i)+j,3*(k)+l)=delTstar_delF(3*(i)+j,3*(k)+l)+ TM(3*(i)+j,3*(a)+b)*delFe_delF(3*(c)+a,3*(k)+l)*FE_tau(c,b);
                                
                            }
                        }
                    }
                    
                }
            }
        }
    }
    
    
    
    FullMatrix<double> PK_Stiff5(dim*dim,dim*dim);
    PK_Stiff5=0.0;
    temp4.reinit(dim,dim);
    temp4.invert(F_tau);
    temp.reinit(dim,dim);
    temp4.mmult(temp,FE_tau);
    temp1.reinit(dim,dim);
    T_star_tau.mTmult(temp1,temp);
    temp2.reinit(dim,dim);
    temp4.mmult(temp2,FE_tau); // Transpose the matrix
    temp3.reinit(dim,dim);
    FE_tau.mmult(temp3,T_star_tau);
    temp5.reinit(dim,dim);
    temp3.mTmult(temp5,F_tau);
    temp6=IdentityMatrix(dim);
    
    for (unsigned int i=0;i<dim;i++){
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                for (unsigned int l=0;l<dim;l++){
                    for (unsigned int a=0;a<dim;a++){
                        for (unsigned int b=0;b<dim;b++){
                            PK_Stiff5(3*(i)+j,3*(k)+l)=PK_Stiff5(3*(i)+j,3*(k)+l)+ temp6(i,a)*delFe_delF(3*(a)+b,3*(k)+l)*temp1(b,j)+FE_tau(i,a)*delTstar_delF(3*(a)+b,3*(k)+l)*temp2(j,b)-temp3(i,a)*delFe_delF(3*(a)+b,3*(k)+l)*temp4(j,b);
                        }
                        PK_Stiff5(3*(i)+j,3*(k)+l)=PK_Stiff5(3*(i)+j,3*(k)+l)-temp5(i,a)*temp4(j,k)*temp4(l,a);
                    }
                    
                }
            }
        }
    }
    
    
    temp.reinit(dim,dim); T_tau.mTmult(temp,rotmat);
    rotmat.mmult(T_tau,temp);
    
    temp.reinit(dim,dim); P_tau.mTmult(temp,rotmat);
    rotmat.mmult(P_tau,temp);
    
    
    dP_dF=0.0;
    FullMatrix<double> L(dim,dim),mn(dim,dim);
    L=0.0;
    temp1.reinit(dim,dim); temp1=IdentityMatrix(dim);
    rotmat.Tmmult(L,temp1);
    
    // Transform the tangent modulus back to crystal frame
    
    
    for(unsigned int m=0;m<dim;m++){
        for(unsigned int n=0;n<dim;n++){
            for(unsigned int o=0;o<dim;o++){
                for(unsigned int p=0;p<dim;p++){
                    for(unsigned int i=0;i<dim;i++){
                        for(unsigned int j=0;j<dim;j++){
                            for(unsigned int k=0;k<dim;k++){
                                for(unsigned int l=0;l<dim;l++){
                                    dP_dF[m][n][o][p]=dP_dF[m][n][o][p]+PK_Stiff5(dim*i+j,dim*k+l)*L(i,m)*L(j,n)*L(k,o)*L(l,p);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    P.reinit(dim,dim);
    P=P_tau;
    T=T_tau;
    
    
    sres_tau.reinit(n_slip_systems);
    sres_tau = s_alpha_tau;
    
    // Update the history variables
    Fe_iter[cellID][quadPtID]=FE_tau;
    Fp_iter[cellID][quadPtID]=FP_tau;
    s_alpha_iter[cellID][quadPtID]=sres_tau;
    
    
}

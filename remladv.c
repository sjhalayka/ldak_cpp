/*
Copyright 2024 Doug Speed.

    LDAK is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    LDAK is distributed in the hope that they will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with LDAK.  If not, see <http://www.gnu.org/licenses/>.

*/

///////////////////////////

//REML for genes with a kinship matrix (basically a reduced version of multi_reml with shortcut=1)
//will have null model (num_regs=0), else normal (num_regs=1 and stats!=NULL) or permutation (num_regs=1 and stats==NULL)

///////////////////////////

double adv_reml(int ns, int num_fixed, int num_regs, double *Y, double *Z, double *U, double *E, double *kintraces, double *X, int Xtotal, double Xsum, double *Xbeta, double *stats, double *vstarts, double tol, int maxiter)
{
int i, i2, j, j2, k, k2, r, g, count, token, one=1, info;
double sum, sumsq, max, alpha, beta;

int total, total2, rflag, *fixed, *fixedsave, nlost, nlost2;
double nfree, varnull, varmin, gam, gam2, gam3, relax;
double likenull, like, likeold, diff;
double *scales, *vars, *vardiffs, *varsds, *hers, *hersds;

double *AI, *AI2, *AI3, *BI, *J, *JAI, *JAIJT;
double *ZTY, *ZTZ, *ZTZ2, *ZTZZTY;
double detV, *ZTVZ, *ZTVZ2, *ZTVZ3, detZTVZ, *PY, **KPY, **PKPY, *traces;

double *UTY, *UTZ, *D, detD, *BUTZ, *H, *HUTY, *HKPY;
double *PX, *XTPY;
double *UTX, *DUTX, detC, *XTVX, *XTVX2, *XTVX3, detXTVX, *F, *FUTZ, *FUTY, *FUTX, *HUTX, *FKPY;

//variables to make compatible with remllikes.c and remlderiv.c
int num_kins=1, Xstarts[1], Xends[1], shortcut=1, num_vects=-9999, ldlt=-9999, constrain=1, memsave=-9999, maxthreads=-9999;
float **Mkins_single=NULL;
double **Mkins=NULL, *sweights, Xsums[1];
char **kinstems=NULL, **ids3=NULL;
sweights=malloc(sizeof(double)*ns);
for(i=0;i<ns;i++){sweights[i]=1;}
if(num_regs==1){Xstarts[0]=0;Xends[0]=Xtotal;Xsums[0]=Xsum;}

size_t scount=0, stotal=0;
int *ipiv, lwork;
double *ZTVY=NULL;
float wkopt_single, *work_single=NULL, alpha_single, beta_single;
float *R_single=NULL, **KR_single=NULL, *V_full=NULL, *V_single=NULL, *RHS_single=NULL;
float *PY_single=NULL, *KPY_single=NULL, *RHS2_single=NULL;
double *V=NULL, *VZ=NULL, *VY=NULL, *VR=NULL, *VZZTVZ=NULL, *P=NULL, *ZTVR=NULL, *PR=NULL, *VKPY=NULL, *ZTVKPY=NULL;
double *kin_diags=NULL;


//set nfree and total
nfree=ns-num_fixed;
total=1+num_kins+num_regs;

//allocate variables

fixed=malloc(sizeof(int)*total);
fixedsave=malloc(sizeof(int)*total);
vars=malloc(sizeof(double)*total);
vardiffs=malloc(sizeof(double)*total);
varsds=malloc(sizeof(double)*total);
scales=malloc(sizeof(double)*total);
hers=malloc(sizeof(double)*total);
hersds=malloc(sizeof(double)*total);

AI=malloc(sizeof(double)*total*total);
AI2=malloc(sizeof(double)*total);
AI3=malloc(sizeof(double)*total*total);
BI=malloc(sizeof(double)*total);
J=malloc(sizeof(double)*total*total);
JAI=malloc(sizeof(double)*total*total);
JAIJT=malloc(sizeof(double)*total*total);

ZTY=malloc(sizeof(double)*num_fixed);
ZTZ=malloc(sizeof(double)*num_fixed*num_fixed);
ZTZ2=malloc(sizeof(double)*num_fixed);
ZTZZTY=malloc(sizeof(double)*num_fixed);

ZTVZ=malloc(sizeof(double)*num_fixed*num_fixed);
ZTVZ2=malloc(sizeof(double)*num_fixed);
ZTVZ3=malloc(sizeof(double)*num_fixed*num_fixed);
PY=malloc(sizeof(double)*ns);
KPY=malloc(sizeof(double*)*total);
for(k=0;k<total;k++){KPY[k]=malloc(sizeof(double)*ns);}
PKPY=malloc(sizeof(double*)*total);
for(k=0;k<total;k++){PKPY[k]=malloc(sizeof(double)*ns);}
traces=malloc(sizeof(double)*total);

UTY=malloc(sizeof(double)*ns);
UTZ=malloc(sizeof(double)*ns*num_fixed);
D=malloc(sizeof(double)*ns);
BUTZ=malloc(sizeof(double)*ns*num_fixed);
H=malloc(sizeof(double)*num_fixed*ns);
HUTY=malloc(sizeof(double)*num_fixed);
HKPY=malloc(sizeof(double)*num_fixed);

if(num_regs==1)
{
PX=malloc(sizeof(double)*ns*Xtotal);
XTPY=malloc(sizeof(double)*Xtotal);

UTX=malloc(sizeof(double)*ns*Xtotal);
DUTX=malloc(sizeof(double)*ns*Xtotal);
XTVX=malloc(sizeof(double)*Xtotal*Xtotal);
XTVX2=malloc(sizeof(double)*Xtotal);
XTVX3=malloc(sizeof(double)*Xtotal*Xtotal);
F=malloc(sizeof(double)*Xtotal*ns);
FUTZ=malloc(sizeof(double)*Xtotal*num_fixed);
FUTY=malloc(sizeof(double)*Xtotal);
FUTX=malloc(sizeof(double)*Xtotal*Xtotal);
HUTX=malloc(sizeof(double)*num_fixed*Xtotal);
FKPY=malloc(sizeof(double)*Xtotal);
}

//fill some variables

//get UTY, UTZ and maybe UTX
alpha=1.0;beta=0.0;
dgemv_("T", &ns, &ns, &alpha, U, &ns, Y, &one, &beta, UTY, &one);
dgemm_("T", "N", &ns, &num_fixed, &ns, &alpha, U, &ns, Z, &ns, &beta, UTZ, &ns);
if(num_regs==1){dgemm_("T", "N", &ns, &Xtotal, &ns, &alpha, U, &ns, X, &ns, &beta, UTX, &ns);}

////////

//solve model with just fixed effects to get varnull and varmin
alpha=1.0;beta=0.0;
dgemv_("T", &ns, &num_fixed, &alpha, Z, &ns, Y, &one, &beta, ZTY, &one);
dgemm_("T", "N", &num_fixed, &num_fixed, &ns, &alpha, Z, &ns, Z, &ns, &beta, ZTZ, &num_fixed);
for(j=0;j<num_fixed;j++){ZTZZTY[j]=ZTY[j];}
(void)eigen_invert(ZTZ, num_fixed, ZTZ2, 1, ZTZZTY, 1);

sumsq=0;
for(i=0;i<ns;i++){sumsq+=pow(Y[i],2);}
for(j=0;j<num_fixed;j++){sumsq-=ZTY[j]*ZTZZTY[j];}
varnull=sumsq/nfree;
varmin=0.0001*varnull;

//set scales
scales[0]=1;
for(k=0;k<num_kins;k++){scales[1+k]=kintraces[k];}
for(r=0;r<num_regs;r++){scales[1+num_kins+r]=1;}

//set starting vars, hers and fixed (remember total=2+num_regs)
if(num_regs==0)	//set heritabilties agnostically
{hers[0]=0.5;hers[1]=.5;}
else	//set based on vstarts
{
if(vstarts[1]>.98){hers[0]=0.01;hers[1]=0.98;hers[2]=0.01;}
else{hers[0]=vstarts[0]-0.01;hers[1]=vstarts[1];hers[2]=0.01;}
}

for(k=0;k<total;k++){vars[k]=hers[k]*varnull/scales[k];}
for(k=0;k<total;k++){fixed[k]=0;}

////////

//now iterate
count=0;
rflag=0;	//0 if normal moves, 1 if reduced moves, 2 if transitioning from reduced to normal moves
while(1)
{
//compute invV, detV, invZTVZ, detZTVZ, P, PY and gam
#include "remllike.c"

//get likelihood
like=-.5*gam-.5*detZTVZ-.5*detV-.5*nfree*log(2*M_PI);

if(count>0)	//set diff and decide what type of move to do
{
if(like>likeold-tol)	//move was fine, so next move will be normal or transitioning
{
diff=like-likeold;
if(rflag==1){rflag=2;}
else{rflag=0;}
}
else	//move was poor, so return to previous state and next move will be reduced
{
for(k=0;k<total;k++){fixed[k]=fixedsave[k];}
for(k=0;k<total;k++){vars[k]-=vardiffs[k];}
sum=0;for(k=0;k<total;k++){sum+=scales[k]*vars[k];}
for(k=0;k<total;k++){hers[k]=scales[k]*vars[k]/sum;}
like=likeold;
diff=0;
rflag=1;
}
}
likeold=like;
for(k=0;k<total;k++){fixedsave[k]=fixed[k];}

//compute PX, XTPY, KPY, PKPY, (inverse) AI and BI
#include "remlderiv.c"

nlost=0;for(k=0;k<total;k++){nlost+=(fixed[k]>=3);}
nlost2=0;for(k=0;k<total;k++){nlost2+=(fixed[k]==1||fixed[k]==2);}

//see if breaking (normally can only break if rflag=0 and nlost2=0, unless at iter limit)

if(nlost>=num_kins+num_regs){break;}	//all heritabilities constrained
if(count>0)
{
if(fabs(diff)<tol&&rflag==0&&nlost2==0){break;}
}
if(count==maxiter)
{printf("Warning, REML failed to converge; consider using \"--max-iter\" and/or \"--tolerance\" to increase the iteration limit and tolerance (currently %d and %.4e)\n", maxiter, tol);break;}

////////

//update variances using NR

//decide how far to move
if(rflag==0||rflag==2){relax=1;}
else{relax*=.5;}

//get proposed moves, ensuring not too large
alpha=relax;beta=0.0;
dgemv_("N", &total, &total, &alpha, AI, &total, BI, &one, &beta, vardiffs, &one);

max=0;
for(k=0;k<total;k++)
{
if(fabs(vardiffs[k])>max){max=fabs(vardiffs[k]);}
}
if(max>.1*varnull)	//then reduce moves
{
relax*=.1*varnull/max;
for(k=0;k<total;k++){vardiffs[k]*=.1*varnull/max;}
}

if(constrain==1)	//variances can not be negative
{
for(k=0;k<total;k++)
{
if(fixed[k]<3)	//free to update
{
if(vars[k]+vardiffs[k]<varmin){vardiffs[k]=varmin-vars[k];fixed[k]++;}
else{fixed[k]=0;}
if(fixed[k]==3){vardiffs[k]=-vars[k];}
}
}}

//now move
for(k=0;k<total;k++){vars[k]+=vardiffs[k];}
sum=0;for(k=0;k<total;k++){sum+=scales[k]*vars[k];}
for(k=0;k<total;k++){hers[k]=scales[k]*vars[k]/sum;}

count++;
}	//end of while loop

////////

if(num_regs==0)	//save heritabilities and likelihood into vstarts
{vstarts[0]=hers[0];vstarts[1]=hers[1];vstarts[2]=like;}
else
{
//have likenull saved in vstarts
likenull=vstarts[2];

if(stats!=NULL)		//analysing real data, get statistics and save
{
//get SEs of hers - Jij=delta*scalei/sum-scalei*scalej*vari/sum^2 (where sum across all elements)
sum=0;for(k=0;k<total;k++){sum+=scales[k]*vars[k];}

for(k=0;k<total;k++)
{
for(k2=0;k2<total;k2++)
{
if(fixed[k]<3&&fixed[k2]<3){J[k+k2*total]=-scales[k]*vars[k]*scales[k2]*pow(sum,-2);}
else{J[k+k2*total]=0;}
}
if(fixed[k]<3){J[k+k*total]+=scales[k]/sum;}
}

alpha=1.0;beta=0.0;
dgemm_("N", "N", &total, &total, &total, &alpha, J, &total, AI, &total, &beta, JAI, &total);
dgemm_("N", "T", &total, &total, &total, &alpha, JAI, &total, J, &total, &beta, JAIJT, &total);

//load up
for(k=1;k<total;k++)
{
if(JAIJT[k+k*total]>=0){hersds[k]=pow(JAIJT[k+k*total],.5);}
else{hersds[k]=-9999;}
}

//load into stats
stats[0]=hers[2];
stats[1]=hersds[2];
stats[2]=likenull;
stats[3]=like;
stats[4]=2*(like-likenull);
if(stats[4]>0){stats[5]=.5*erfc(pow(stats[4],.5)*M_SQRT1_2);}
else{stats[5]=.75;}

for(i=0;i<ns;i++){Xbeta[i]=vars[2]*KPY[2][i];}
}
}

free(sweights);
free(fixed);free(fixedsave);free(vars);free(vardiffs);free(varsds);free(scales);free(hers);free(hersds);
free(AI);free(AI2);free(AI3);free(BI);free(J);free(JAI);free(JAIJT);
free(ZTY);free(ZTZ);free(ZTZ2);free(ZTZZTY);
free(ZTVZ);free(ZTVZ2);free(ZTVZ3);free(PY);
for(k=0;k<total;k++){free(KPY[k]);free(PKPY[k]);}free(KPY);free(PKPY);free(traces);
free(UTY);free(UTZ);free(D);free(BUTZ);free(H);free(HUTY);free(HKPY);
if(num_regs==1){free(PX);free(XTPY);free(UTX);free(DUTX);free(XTVX);free(XTVX2);free(XTVX3);free(F);free(FUTZ);free(FUTY);free(FUTX);free(HUTX);free(FKPY);}

return(2*(like-likenull));
}	//end of adv_reml

///////////////////////////


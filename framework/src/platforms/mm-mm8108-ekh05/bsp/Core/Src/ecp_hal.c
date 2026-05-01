/*
 *  Elliptic curves over GF(p): generic functions
 *
 *  Copyright The Mbed TLS Contributors
 *  Copyright (C) 2021, STMicroelectronics, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file implements STMicroelectronics EC scalar mul and point check
 *  with HW services based on mbed TLS API
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ECP_C)

#define MBEDTLS_ALLOW_PRIVATE_ACCESS 1

#include "mbedtls/ecp.h"
#include "mbedtls/threading.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#if defined(MBEDTLS_ECP_ALT)

#include "stm32u5xx_hal.h"

#define ST_ECP_TIMEOUT (5000U)

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * Multiplication using the comb method - for curves in short Weierstrass form
 *
 * This function is mainly responsible for administrative work:
 * - managing the restart context if enabled
 * - managing the table of precomputed points (passed between the below two
 *   functions): allocation, computation, ownership transfer, freeing.
 *
 * It delegates the actual arithmetic work to:
 *      ecp_precompute_comb() and ecp_mul_comb_with_precomp()
 *
 * See comments on ecp_comb_recode_core() regarding the computation strategy.
 *
 * STMicroelectronics edition
 *
 */
int ecp_mul_comb(mbedtls_ecp_group *grp,
                 mbedtls_ecp_point *R,
                 const mbedtls_mpi *m,
                 const mbedtls_ecp_point *P,
                 int (*f_rng)(void *, unsigned char *, size_t),
                 void *p_rng,
                 mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t olen;
    size_t scalarMulSize;
    uint8_t *P_binary;
    uint8_t *m_binary = NULL;
    uint8_t *R_binary = NULL;
    PKA_HandleTypeDef hpka = { 0 };
    PKA_ECCMulInTypeDef ECC_MulIn = { 0 };
    PKA_ECCMulOutTypeDef ECC_MulOut;

    (void)f_rng;
    (void)p_rng;
    (void)rs_ctx;

    /* Set HW peripheral input parameter: scalar m size.
     * Round size up to 4-byte boundary, as the PKA HAL requires
     * a word-sized buffers and will round up the size internally. */
    scalarMulSize = mbedtls_mpi_size(m);
    scalarMulSize = (((scalarMulSize + 3) >> 2) << 2);
    ECC_MulIn.scalarMulSize = scalarMulSize;

    /* Set HW peripheral Input parameter: curve coefs */
    ECC_MulIn.modulusSize = grp->st_modulus_size;
    ECC_MulIn.coefSign = grp->st_a_sign;
    ECC_MulIn.coefA = grp->st_a_abs;
    ECC_MulIn.modulus = grp->st_p;
    ECC_MulIn.coefB = grp->st_b;
    ECC_MulIn.primeOrder = grp->st_n;

    /* Set HW peripheral input parameter: coordinates of P point */
    P_binary = (uint8_t *)mbedtls_calloc(2U * grp->st_modulus_size + 1U, sizeof(uint8_t));
    MBEDTLS_MPI_CHK((P_binary == NULL) ? MBEDTLS_ERR_ECP_ALLOC_FAILED : 0);

    MBEDTLS_MPI_CHK(mbedtls_ecp_point_write_binary(grp,
                                                   P,
                                                   MBEDTLS_ECP_PF_UNCOMPRESSED,
                                                   &olen,
                                                   P_binary,
                                                   2U * grp->st_modulus_size + 1U));

    ECC_MulIn.pointX = P_binary + 1U;
    ECC_MulIn.pointY = P_binary + grp->st_modulus_size + 1U;

    /* Set HW peripheral input parameter: scalar m */
    m_binary = (uint8_t *)mbedtls_calloc(scalarMulSize, sizeof(uint8_t));
    MBEDTLS_MPI_CHK((m_binary == NULL) ? MBEDTLS_ERR_ECP_ALLOC_FAILED : 0);

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(m, m_binary, scalarMulSize));
    ECC_MulIn.scalarMul = m_binary;

    /* Enable HW peripheral clock */
    __HAL_RCC_PKA_CLK_ENABLE();

    /* Initialize HW peripheral */
    hpka.Instance = PKA;
    MBEDTLS_MPI_CHK((HAL_PKA_Init(&hpka) != HAL_OK) ? MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED : 0);

    /* Reset PKA RAM */
    HAL_PKA_RAMReset(&hpka);

    /* Launch the scalar multiplication */
    MBEDTLS_MPI_CHK((HAL_PKA_ECCMul(&hpka, &ECC_MulIn, ST_ECP_TIMEOUT) != HAL_OK) ?
                        MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED :
                        0);

    /* Allocate memory space for scalar multiplication result */
    R_binary = (uint8_t *)mbedtls_calloc(2U * grp->st_modulus_size + 1U, sizeof(uint8_t));
    MBEDTLS_MPI_CHK((R_binary == NULL) ? MBEDTLS_ERR_ECP_ALLOC_FAILED : 0);

    ECC_MulOut.ptX = R_binary + 1U;
    ECC_MulOut.ptY = R_binary + grp->st_modulus_size + 1U;

    /* Get the scalar multiplication result */
    HAL_PKA_ECCMul_GetResult(&hpka, &ECC_MulOut);

    /* Convert the scalar multiplication result into ecp point format */
    R_binary[0] = 0x04U;
    MBEDTLS_MPI_CHK(
        mbedtls_ecp_point_read_binary(grp, R, R_binary, 2U * grp->st_modulus_size + 1U));

cleanup:
    /* De-initialize HW peripheral */
    HAL_PKA_DeInit(&hpka);

    /* Disable HW peripheral clock */
    __HAL_RCC_PKA_CLK_DISABLE();

    /* Free memory */
    if (P_binary != NULL)
    {
        mbedtls_platform_zeroize(P_binary, 2U * grp->st_modulus_size + 1U);
        mbedtls_free(P_binary);
    }

    if (m_binary != NULL)
    {
        mbedtls_platform_zeroize(m_binary, scalarMulSize);
        mbedtls_free(m_binary);
    }

    if (R_binary != NULL)
    {
        mbedtls_platform_zeroize(R_binary, 2U * grp->st_modulus_size + 1U);
        mbedtls_free(R_binary);
    }

    return ret;
}

/*
 * Check that an affine point is valid as a public key,
 * short weierstrass curves (SEC1 3.2.3.1)
 *
 * STMicroelectronics edition
 *
 */
int ecp_check_pubkey_sw(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t olen;
    uint8_t *pt_binary;
    PKA_HandleTypeDef hpka = { 0 };
    PKA_PointCheckInTypeDef ECC_PointCheck = { 0 };
    PKA_MontgomeryParamInTypeDef inp = { 0 };
    uint8_t *pt_montgomery = NULL;

    /* pt coordinates must be normalized for our checks */
    if (mbedtls_mpi_cmp_int(&pt->MBEDTLS_PRIVATE(X), 0) < 0 ||
        mbedtls_mpi_cmp_int(&pt->MBEDTLS_PRIVATE(Y), 0) < 0 ||
        mbedtls_mpi_cmp_mpi(&pt->MBEDTLS_PRIVATE(X), &grp->P) >= 0 ||
        mbedtls_mpi_cmp_mpi(&pt->MBEDTLS_PRIVATE(Y), &grp->P) >= 0)
    {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

    /* Set HW peripheral Input parameter: curve coefs */
    ECC_PointCheck.modulusSize = grp->st_modulus_size;
    ECC_PointCheck.modulus = grp->st_p;
    ECC_PointCheck.coefSign = grp->st_a_sign;
    ECC_PointCheck.coefA = grp->st_a_abs;
    ECC_PointCheck.coefB = grp->st_b;

    /* Set HW peripheral input parameter: coordinates of point to check */
    pt_binary = (uint8_t *)mbedtls_calloc((2U * grp->st_modulus_size) + 1U, sizeof(uint8_t));
    MBEDTLS_MPI_CHK((pt_binary == NULL) ? MBEDTLS_ERR_ECP_ALLOC_FAILED : 0);

    pt_montgomery = (uint8_t *)mbedtls_calloc(grp->st_modulus_size, sizeof(uint8_t));
    MBEDTLS_MPI_CHK((pt_montgomery == NULL) ? MBEDTLS_ERR_ECP_ALLOC_FAILED : 0);

    MBEDTLS_MPI_CHK(mbedtls_ecp_point_write_binary(grp,
                                                   pt,
                                                   MBEDTLS_ECP_PF_UNCOMPRESSED,
                                                   &olen,
                                                   pt_binary,
                                                   (2U * grp->st_modulus_size) + 1U));

    ECC_PointCheck.pointX = pt_binary + 1U;
    ECC_PointCheck.pointY = pt_binary + grp->st_modulus_size + 1U;

    /* Enable HW peripheral clock */
    __HAL_RCC_PKA_CLK_ENABLE();

    /* Initialize HW peripheral */
    hpka.Instance = PKA;
    MBEDTLS_MPI_CHK((HAL_PKA_Init(&hpka) != HAL_OK) ? MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED : 0);

    /* Reset PKA RAM */
    HAL_PKA_RAMReset(&hpka);

    /* Set Montgomery R2 input parameters */
    inp.size = grp->st_modulus_size;
    inp.pOp1 = grp->st_p;

    /* Launch the processing */
    MBEDTLS_MPI_CHK((HAL_PKA_MontgomeryParam(&hpka, &inp, ST_ECP_TIMEOUT) != HAL_OK) ?
                        MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED :
                        0);

    /* Get Montgomery R2 parameters */
    HAL_PKA_MontgomeryParam_GetResult(&hpka, (uint32_t *)pt_montgomery);
    ECC_PointCheck.pMontgomeryParam = (uint32_t *)pt_montgomery;

    /* Launch the point check */
    MBEDTLS_MPI_CHK((HAL_PKA_PointCheck(&hpka, &ECC_PointCheck, ST_ECP_TIMEOUT) != HAL_OK) ?
                        MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED :
                        0);

    /* Get the result of the point check */
    if (HAL_PKA_PointCheck_IsOnCurve(&hpka) != 1U)
    {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
    }

cleanup:
    /* De-initialize HW peripheral */
    HAL_PKA_DeInit(&hpka);

    /* Disable HW peripheral clock */
    __HAL_RCC_PKA_CLK_DISABLE();

    /* Free memory */
    if (pt_binary != NULL)
    {
        mbedtls_platform_zeroize(pt_binary, (2U * grp->st_modulus_size) + 1U);
        mbedtls_free(pt_binary);
    }

    if (pt_montgomery != NULL)
    {
        mbedtls_platform_zeroize(pt_montgomery, grp->st_modulus_size);
        mbedtls_free(pt_montgomery);
    }

    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#endif /* MBEDTLS_ECP_ALT */

#endif /* MBEDTLS_ECP_C */

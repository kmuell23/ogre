/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
module;

#include <cassert>
#include <cstring>

export module Ogre.Core:Matrix3;

export import :Math;
export import :Prerequisites;
export import :Vector;

export import <ostream>;

// NB All code adapted from Wild Magic 0.2 Matrix math (free source code)
// http://www.geometrictools.com/
// NOTE.  The (x,y,z) coordinate system is assumed to be right-handed.
// Coordinate axis rotation matrices are of the form
//   RX =    1       0       0
//           0     cos(t) -sin(t)
//           0     sin(t)  cos(t)
// where t > 0 indicates a counterclockwise rotation in the yz-plane
//   RY =  cos(t)    0     sin(t)
//           0       1       0
//        -sin(t)    0     cos(t)
// where t > 0 indicates a counterclockwise rotation in the zx-plane
//   RZ =  cos(t) -sin(t)    0
//         sin(t)  cos(t)    0
//           0       0       1
// where t > 0 indicates a counterclockwise rotation in the xy-plane.
export
namespace Ogre
{
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Math
    *  @{
    */
    /** A 3x3 matrix which can represent rotations around axes.
        @note
            <b>All the code is adapted from the Wild Magic 0.2 Matrix
            library (http://www.geometrictools.com/).</b>
        @par
            The coordinate system is assumed to be <b>right-handed</b>.
    */
    struct Matrix3
    {
        /// Member access, allows use of construct mat[r][c]
        auto operator[] (size_t iRow) const -> const Real*
        {
            return m[iRow];
        }

        auto operator[] (size_t iRow) -> Real*
        {
            return m[iRow];
        }

        [[nodiscard]] auto GetColumn(size_t iCol) const -> Vector3
        {
            assert(iCol < 3);
            return {m[0][iCol], m[1][iCol], m[2][iCol]};
        }
        void SetColumn(size_t iCol, const Vector3& vec)
        {
            assert(iCol < 3);
            m[0][iCol] = vec.x;
            m[1][iCol] = vec.y;
            m[2][iCol] = vec.z;
        }
        void FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
        {
            SetColumn(0, xAxis);
            SetColumn(1, yAxis);
            SetColumn(2, zAxis);
        }

        /** Tests 2 matrices for equality.
         */
        [[nodiscard]] auto operator== (const Matrix3& rkMatrix) const noexcept -> bool = default;

        // arithmetic operations
        /** Matrix addition.
         */
        auto operator+ (const Matrix3& rkMatrix) const -> Matrix3;

        /** Matrix subtraction.
         */
        auto operator- (const Matrix3& rkMatrix) const -> Matrix3;

        /** Matrix concatenation using '*'.
         */
        auto operator* (const Matrix3& rkMatrix) const -> Matrix3;
        auto operator- () const -> Matrix3;

        /// Vector * matrix [1x3 * 3x3 = 1x3]
        friend auto operator* (const Vector3& rkVector,
            const Matrix3& rkMatrix) -> Vector3;

        /// Matrix * scalar
        auto operator* (Real fScalar) const -> Matrix3;

        /// Scalar * matrix
        friend auto operator* (Real fScalar, const Matrix3& rkMatrix) -> Matrix3;

        // utilities
        [[nodiscard]] auto Transpose () const -> Matrix3;
        auto Inverse (Matrix3& rkInverse, Real fTolerance = 1e-06f) const -> bool;
        [[nodiscard]] auto Inverse (Real fTolerance = 1e-06f) const -> Matrix3;
        [[nodiscard]] auto Determinant() const -> Real { return determinant(); }

        [[nodiscard]] auto transpose() const -> Matrix3 { return Transpose(); }
        [[nodiscard]] auto inverse() const -> Matrix3 { return Inverse(); }
        [[nodiscard]] auto determinant() const -> Real
        {
            Real fCofactor00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
            Real fCofactor10 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
            Real fCofactor20 = m[1][0] * m[2][1] - m[1][1] * m[2][0];

            return m[0][0] * fCofactor00 + m[0][1] * fCofactor10 + m[0][2] * fCofactor20;
        }

        /** Determines if this matrix involves a negative scaling. */
        [[nodiscard]] auto hasNegativeScale() const -> bool { return determinant() < 0; }

        /// Singular value decomposition
        void SingularValueDecomposition (Matrix3& rkL, Vector3& rkS,
            Matrix3& rkR) const;
        void SingularValueComposition (const Matrix3& rkL,
            const Vector3& rkS, const Matrix3& rkR);

        /// Gram-Schmidt orthogonalisation (applied to columns of rotation matrix)
        [[nodiscard]] auto orthonormalised() const -> Matrix3
        {
            // Algorithm uses Gram-Schmidt orthogonalisation.  If 'this' matrix is
            // M = [m0|m1|m2], then orthonormal output matrix is Q = [q0|q1|q2],
            //
            //   q0 = m0/|m0|
            //   q1 = (m1-(q0*m1)q0)/|m1-(q0*m1)q0|
            //   q2 = (m2-(q0*m2)q0-(q1*m2)q1)/|m2-(q0*m2)q0-(q1*m2)q1|
            //
            // where |V| indicates length of vector V and A*B indicates dot
            // product of vectors A and B.

            Matrix3 Q;
            // compute q0
            Q.SetColumn(0, GetColumn(0) / GetColumn(0).length());

            // compute q1
            Real dot0 = Q.GetColumn(0).dotProduct(GetColumn(1));
            Q.SetColumn(1, (GetColumn(1) - dot0 * Q.GetColumn(0)).normalisedCopy());

            // compute q2
            Real dot1 = Q.GetColumn(1).dotProduct(GetColumn(2));
            dot0 = Q.GetColumn(0).dotProduct(GetColumn(2));
            Q.SetColumn(2, (GetColumn(2) - dot0 * Q.GetColumn(0) + dot1 * Q.GetColumn(1)).normalisedCopy());

            return Q;
        }

        /// Orthogonal Q, diagonal D, upper triangular U stored as (u01,u02,u12)
        void QDUDecomposition (Matrix3& rkQ, Vector3& rkD,
            Vector3& rkU) const;

        [[nodiscard]] auto SpectralNorm () const -> Real;

        /// Note: Matrix must be orthonormal
        void ToAngleAxis (Vector3& rkAxis, Radian& rfAngle) const;
        inline void ToAngleAxis (Vector3& rkAxis, Degree& rfAngle) const {
            Radian r;
            ToAngleAxis ( rkAxis, r );
            rfAngle = r;
        }
        void FromAngleAxis (const Vector3& rkAxis, const Radian& fRadians);

        /**
            @name Euler angle conversions
            (De-)composes the matrix in/ from yaw, pitch and roll angles,
            where yaw is rotation about the Y vector, pitch is rotation about the
            X axis, and roll is rotation about the Z axis.

            The function suffix indicates the (de-)composition order;
            e.g. with the YXZ variants the matrix will be (de-)composed as yaw*pitch*roll

            For ToEulerAngles*, the return value denotes whether the solution is unique.
            @note The matrix to be decomposed must be orthonormal.
            @{
        */
        auto ToEulerAnglesXYZ(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        auto ToEulerAnglesXZY(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        auto ToEulerAnglesYXZ(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        auto ToEulerAnglesYZX(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        auto ToEulerAnglesZXY(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        auto ToEulerAnglesZYX(Radian& rfYAngle, Radian& rfPAngle, Radian& rfRAngle) const -> bool;
        void FromEulerAnglesXYZ (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        void FromEulerAnglesXZY (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        void FromEulerAnglesYXZ (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        void FromEulerAnglesYZX (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        void FromEulerAnglesZXY (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        void FromEulerAnglesZYX (const Radian& fYAngle, const Radian& fPAngle, const Radian& fRAngle);
        /// @}
        /// Eigensolver, matrix must be symmetric
        void EigenSolveSymmetric (Real afEigenvalue[3],
            Vector3 akEigenvector[3]) const;

        static void TensorProduct (const Vector3& rkU, const Vector3& rkV,
            Matrix3& rkProduct);

        /** Determines if this matrix involves a scaling. */
        [[nodiscard]] auto hasScale() const -> bool
        {
            // check magnitude of column vectors (==local axes)
            Real t = m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0];
            if (!Math::RealEqual(t, 1.0, (Real)1e-04))
                return true;
            t = m[0][1] * m[0][1] + m[1][1] * m[1][1] + m[2][1] * m[2][1];
            if (!Math::RealEqual(t, 1.0, (Real)1e-04))
                return true;
            t = m[0][2] * m[0][2] + m[1][2] * m[1][2] + m[2][2] * m[2][2];
            if (!Math::RealEqual(t, 1.0, (Real)1e-04))
                return true;

            return false;
        }

        /** Function for writing to a stream.
        */
        inline friend auto operator <<
            ( std::ostream& o, const Matrix3& mat ) -> std::ostream&
        {
            o << "Matrix3(" << mat[0][0] << ", " << mat[0][1] << ", " << mat[0][2] << "; "
                            << mat[1][0] << ", " << mat[1][1] << ", " << mat[1][2] << "; "
                            << mat[2][0] << ", " << mat[2][1] << ", " << mat[2][2] << ")";
            return o;
        }

        static const Real EPSILON;
        static const Matrix3 ZERO;
        static const Matrix3 IDENTITY;

        // support for eigensolver
        void Tridiagonal (Real afDiag[3], Real afSubDiag[3]);
        auto QLAlgorithm (Real afDiag[3], Real afSubDiag[3]) -> bool;

        // support for singular value decomposition
        static const unsigned int msSvdMaxIterations;
        static void Bidiagonalize (Matrix3& kA, Matrix3& kL,
            Matrix3& kR);
        static void GolubKahanStep (Matrix3& kA, Matrix3& kL,
            Matrix3& kR);

        // support for spectral norm
        static auto MaxCubicRoot (Real afCoeff[3]) -> Real;

        Real m[3][3];
    };

    /// Matrix * vector [3x3 * 3x1 = 3x1]
    inline auto operator*(const Matrix3& m, const Vector3& v) -> Vector3
    {
        return {
                m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
    }

    inline auto Math::lookRotation(const Vector3& direction, const Vector3& yaw) -> Matrix3
    {
        Matrix3 ret;
        // cross twice to rederive, only direction is unaltered
        const Vector3& xAxis = yaw.crossProduct(direction).normalisedCopy();
        const Vector3& yAxis = direction.crossProduct(xAxis);
        ret.FromAxes(xAxis, yAxis, direction);
        return ret;
    }
    /** @} */
    /** @} */
}
static_assert(std::is_aggregate_v<Ogre::Matrix3>);
static_assert(std::is_trivial_v<Ogre::Matrix3>);
static_assert(std::is_standard_layout_v<Ogre::Matrix3>);

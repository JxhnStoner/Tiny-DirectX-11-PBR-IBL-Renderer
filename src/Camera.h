#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>

using namespace DirectX;

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    XMFLOAT3 Position;
    XMFLOAT3 Front;
    XMFLOAT3 Up;
    XMFLOAT3 Right;
    XMFLOAT3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // constructor with vectors
    Camera(XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(XMFLOAT3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(XMFLOAT3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = XMFLOAT3(posX, posY, posZ);
        WorldUp = XMFLOAT3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    XMMATRIX GetViewMatrix()
    {
        return XMMatrixLookAtLH(XMLoadFloat3(&Position), XMLoadFloat3(&Position) + XMLoadFloat3(&Front), XMLoadFloat3(&Up));
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(CameraMovement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;

        XMVECTOR Pos = XMLoadFloat3(&Position);
        if (direction == CameraMovement::FORWARD)
            Pos += XMLoadFloat3(&Front) * velocity;
        if (direction == CameraMovement::BACKWARD)
            Pos -= XMLoadFloat3(&Front) * velocity;
        if (direction == CameraMovement::LEFT)
            Pos += XMLoadFloat3(&Right) * velocity;
        if (direction == CameraMovement::RIGHT)
            Pos -= XMLoadFloat3(&Right) * velocity;
        XMStoreFloat3(&Position, Pos);
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw -= xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        XMFLOAT3 front;
        front.x = cos(XMConvertToRadians(Yaw)) * cos(XMConvertToRadians(Pitch));
        front.y = sin(XMConvertToRadians(Pitch));
        front.z = sin(XMConvertToRadians(Yaw)) * cos(XMConvertToRadians(Pitch));
        XMStoreFloat3(&Front,XMVector3Normalize(XMLoadFloat3(&front)));
        // also re-calculate the Right and Up vector
        XMStoreFloat3(&Right, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&Front), XMLoadFloat3(&WorldUp)))); // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        XMStoreFloat3(&Up, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&Right), XMLoadFloat3(&Front))));
    }
};
#endif

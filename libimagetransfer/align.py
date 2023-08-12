# Based on https://learnopencv.com/image-alignment-feature-based-using-opencv-c-python/
# import cv2
# import numpy as np
 
# def alignImages(im1, im2, max_features=500, good_match_percent=0.15):
 
#   # Convert images to grayscale
#   im1Gray = cv2.cvtColor(im1, cv2.COLOR_BGR2GRAY)
#   im2Gray = cv2.cvtColor(im2, cv2.COLOR_BGR2GRAY)
 
#   # Detect ORB features and compute descriptors.
#   orb = cv2.ORB_create(max_features)
#   keypoints1, descriptors1 = orb.detectAndCompute(im1Gray, None)
#   keypoints2, descriptors2 = orb.detectAndCompute(im2Gray, None)
 
#   # Match features.
#   matcher = cv2.DescriptorMatcher_create(cv2.DESCRIPTOR_MATCHER_BRUTEFORCE_HAMMING)
#   matches = matcher.match(descriptors1, descriptors2, None)
 
#   # Sort matches by score
#   matches.sort(key=lambda x: x.distance, reverse=False)
 
#   # Remove not so good matches
#   numGoodMatches = int(len(matches) * good_match_percent)
#   matches = matches[:numGoodMatches]
 
#   # Draw top matches
#   imMatches = cv2.drawMatches(im1, keypoints1, im2, keypoints2, matches, None)
#   cv2.imwrite("matches.jpg", imMatches)
 
#   # Extract location of good matches
#   points1 = np.zeros((len(matches), 2), dtype=np.float32)
#   points2 = np.zeros((len(matches), 2), dtype=np.float32)
 
#   for i, match in enumerate(matches):
#     points1[i, :] = keypoints1[match.queryIdx].pt
#     points2[i, :] = keypoints2[match.trainIdx].pt
 
#   # Find homography
#   h, mask = cv2.findHomography(points1, points2, cv2.RANSAC)
 
#   # Use homography
#   height, width, _ = im2.shape
#   im1Reg = cv2.warpPerspective(im1, h, (width, height))
 
#   return im1Reg, h
 

# Adapted from https://docs.opencv.org/3.4/d1/de0/tutorial_py_feature_homography.html
import numpy as np
import cv2
# from matplotlib import pyplot as plt

OpenCvImage = np.ndarray

# img1 = cv.imread('box.png', cv.IMREAD_GRAYSCALE) # queryImage
# img2 = cv.imread('box_in_scene.png', cv.IMREAD_GRAYSCALE) # trainImage

def find_alignment_transform(base_img_gray: OpenCvImage, img_to_transform_gray: OpenCvImage) -> cv2.Mat:
    """
    Take two images - a base image, and an image-to-transform - and return an affine matrix M that aligns image-to-transform with base.
    """
    # DONT USE warpPerspective with the result - use warpAffine
    # rows, cols = base_img_gray.shape[:2]
    # # cv2.warpPerspective(img_to_transform, M, (cols, rows))
    # cv2.warpAffine(img_to_transform, M, (cols, rows))

    MIN_MATCH_COUNT = 10

    # Use Scale-Invariant Feature Transform to find key points in both images we can use for alignment
    # https://docs.opencv.org/3.4/da/df5/tutorial_py_sift_intro.html
    # Initiate SIFT detector
    sift = cv2.SIFT_create()
    # find the keypoints and descriptors with SIFT
    kp1, des1 = sift.detectAndCompute(base_img_gray, None)
    kp2, des2 = sift.detectAndCompute(img_to_transform_gray, None)

    # Use FLANN - Fast Library for Approximate Nearest Neighbors - to find matches between the src and dest keypoints
    FLANN_INDEX_KDTREE = 1
    index_params = {
        'algorithm': FLANN_INDEX_KDTREE,
        'trees': 5
    }
    search_params = {
        'checks': 50
    }
    flann = cv2.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1, des2, k=2)
    # Find all the good matches as per Lowe's ratio test.
    # Lowe's ratio test explained: https://stackoverflow.com/a/60343973/4248422
    # Basically, instead of finding the single best match from a descriptor in A to a descriptor in B,
    # find the top two matches for A -> B. Assert that the best match is only actually accurate if it's a MUCH better match then the second best.
    # If there are two roughly equally good Bs for A, then we shouldn't use A for matching at all.
    good_matches = [
        m
        for m, n in matches
        if m.distance < 0.7*n.distance
    ]

    if len(good_matches) > MIN_MATCH_COUNT:
        # Create a Nx1x2 array of floats to represent the good matching points in the source and destination
        src_pts = np.float32([ kp1[m.queryIdx].pt for m in good_matches ]).reshape(-1,1,2)
        dst_pts = np.float32([ kp2[m.trainIdx].pt for m in good_matches ]).reshape(-1,1,2)

        # Fit a perspective projection matrix to transform dst_pts -> src_pts.
        # We don't actually need a perspective matrix, so estimate a 2D one instead
        # transform_to_base, mask = cv.findHomography(src_pts, dst_pts, cv.RANSAC, 5.0)
        transform_to_base, _ = cv2.estimateAffinePartial2D(dst_pts, src_pts)

        return transform_to_base
    else:
        raise RuntimeError("Couldn't find enough matches")
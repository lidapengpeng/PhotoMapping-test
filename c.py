import csv
import os
import shutil


def process_csv(csv_path):
    # 创建 images 目录，如果不存在
    base_path = os.path.join(os.path.dirname(csv_path), 'images')
    if not os.path.exists(base_path):
        os.makedirs(base_path)

    with open(csv_path, newline='', encoding='utf-8') as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader)  # 读取第一行作为列标题

        # 确定 image path 和 photo index 的列索引
        photo_index_index = headers.index("Photo Index")
        image_path_index = headers.index(" Image Path")
        # 创建每个瓦片名称对应的文件夹和文本文件
        tile_folders = {}
        tile_files = {}
        for i, header in enumerate(headers):
            if i > image_path_index:
                folder_path = os.path.join(base_path, header)
                os.makedirs(folder_path, exist_ok=True)
                tile_folders[i] = folder_path
                # 创建对应的文本文件
                tile_files[i] = open(os.path.join(folder_path, 'photo_indices.txt'), 'w')

        # 遍历文件的每一行
        for row in reader:
            image_path = row[image_path_index]
            photo_index = row[photo_index_index]
            for i in range(image_path_index + 1, len(headers)):
                try:
                    percentage = float(row[i])
                    if percentage > 20:
                        # 复制文件到对应的文件夹
                        target_folder = tile_folders[i]
                        target_path = os.path.join(target_folder, os.path.basename(image_path))
                        shutil.copy(image_path, target_path)
                        print(f"Copied {image_path} to {target_path}")
                        # 记录 photo index 到文本文件
                        tile_files[i].write(f"{photo_index}\n")
                except ValueError:
                    continue  # 如果转换失败或没有找到文件，则忽略

        # 关闭所有打开的文本文件
        for file in tile_files.values():
            file.close()


# 用法
csv_file_path = 'output.csv'  # 这里填写你的 CSV 文件的绝对路径
process_csv(csv_file_path)
